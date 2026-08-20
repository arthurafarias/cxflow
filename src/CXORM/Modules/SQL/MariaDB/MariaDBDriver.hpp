// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include "CXORM/Core/Logging/LoggerManager.hpp"
#include "CXORM/Core/SharedPointer.hpp"
#include "CXORM/Modules/SQL/Base/AbstractDriver.hpp"
#include "CXORM/Modules/SQL/Base/QueryBuilder.hpp"
#include "CXORM/Modules/SQL/Base/QueryResult.hpp"
#include "CXORM/Modules/SQL/MariaDB/MariaDBCursor.hpp"
#include "CXORM/Modules/SQL/MariaDB/MariaDBStatement.hpp"
#include <CXORM/Core/Containers/String.hpp>
#include <CXORM/Core/Exceptions/RuntimeException.hpp>
#include <CXORM/Utils/Patterns/Creator.hpp>

#include <mysql.h>
#include <utility>

using namespace CXORM::Utils::Patterns;

using namespace CXORM::Core::Containers;
using namespace CXORM::Base;

namespace CXORM::MariaDB {

class MariaDBDriver : public Creator<MariaDBDriver, AbstractDriver>,
                      public EnableSharedFromThis<MariaDBDriver>,
                      public AbstractDriver {
public:
  MariaDBDriver(const String &host, const String &user, const String &password,
                const String &database, unsigned int port = 3306)
      : host(host), user(user), password(password), database(database), port(port) {

    connection = mysql_init(nullptr);

    if (connection == nullptr) {
      throw Core::Exceptions::RuntimeException(
          "Failed to allocate a MariaDB connection handle");
    }

    if (mysql_real_connect(connection, host.c_str(), user.c_str(), password.c_str(),
                           database.c_str(), port, nullptr, 0) == nullptr) {
      Core::Exceptions::RuntimeException error(
          "Failed to connect to MariaDB at {}:{}: {}", host.c_str(), port,
          mysql_error(connection));
      mysql_close(connection);
      connection = nullptr;
      throw error;
    }
  }

  virtual ~MariaDBDriver() {
    if (connection != nullptr) {
      mysql_close(connection);
    }
  }

  // Unlike SQLiteDriver (raw owning pointer, destructor, but no copy/move -
  // see the note in SQLiteCursorTest.hpp), MariaDBDriver actually declares
  // these, so a copy is a compile error instead of a double mysql_close().
  MariaDBDriver(const MariaDBDriver &) = delete;
  MariaDBDriver &operator=(const MariaDBDriver &) = delete;

  MariaDBDriver(MariaDBDriver &&other) noexcept
      : host(std::move(other.host)), user(std::move(other.user)),
        password(std::move(other.password)), database(std::move(other.database)),
        port(other.port), connection(other.connection) {
    other.connection = nullptr;
  }

  MariaDBDriver &operator=(MariaDBDriver &&other) noexcept {
    if (this != &other) {
      if (connection != nullptr) {
        mysql_close(connection);
      }
      host = std::move(other.host);
      user = std::move(other.user);
      password = std::move(other.password);
      database = std::move(other.database);
      port = other.port;
      connection = other.connection;
      other.connection = nullptr;
    }
    return *this;
  }

  virtual QueryResult query(SharedPointer<CXORM::Base::QueryBuilder> query) override {
    QueryResult collection = QueryResult::make();
    auto compiled = query->compile();

    Core::Logging::LoggerManager::info("{}", compiled.c_str());

    auto result = run_unbuffered_query(connection, compiled);

    if (result == nullptr) {
      return collection;
    }

    unsigned int columns = mysql_num_fields(result.get());
    MYSQL_FIELD *fields = mysql_fetch_fields(result.get());

    for (MYSQL_ROW row = mysql_fetch_row(result.get()); row != nullptr;
         row = mysql_fetch_row(result.get())) {
      auto el = SharedPointer<Map<String, String>>::make();

      for (unsigned int i = 0; i < columns; i++) {
        // Unlike SQLiteDriver::query()'s callback (see
        // SQLiteDriverTest::SelectingARowWithANullColumnThrowsOrCrashes_KnownCriticalDefect),
        // a NULL column is explicitly checked for here.
        (*el)[fields[i].name] = row[i] != nullptr ? row[i] : "";
      }

      collection->push_back(el);
    }

    if (mysql_errno(connection) != 0) {
      throw Core::Exceptions::RuntimeException("Failed to fetch results for '{}': {}",
                                               compiled.c_str(), mysql_error(connection));
    }

    return collection;
  }

  // Lazy counterpart to query(): streams the result set row by row via
  // mysql_use_result() instead of collecting everything up front - see
  // MariaDBCursor.
  MariaDBCursor cursor(SharedPointer<CXORM::Base::QueryBuilder> query) {
    auto compiled = query->compile();
    Core::Logging::LoggerManager::info("{}", compiled.c_str());
    return MariaDBCursor(connection, compiled);
  }

private:
  String host;
  String user;
  String password;
  String database;
  unsigned int port;
  MYSQL *connection = nullptr;
};

} // namespace CXORM::MariaDB
