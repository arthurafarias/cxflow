// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include "CXORM/Core/Containers/Map.hpp"
#include "CXORM/Core/Containers/String.hpp"
#include "CXORM/Core/Exceptions/RuntimeException.hpp"
#include "CXORM/Modules/SQL/MariaDB/MariaDBStatement.hpp"

#include <iterator>
#include <mysql.h>
#include <ranges>
#include <utility>

using namespace CXORM::Core::Containers;

namespace CXORM::MariaDB {

// Same shape as CXORM::SQLite::Row - keyed by column name - so code moving
// between backends doesn't need to learn a second row type.
using Row = Map<String, String>;

// A lazy view over a SELECT's result set, mirroring
// CXORM::SQLite::SQLiteCursor: rows are pulled from the server one
// mysql_fetch_row() at a time (via mysql_use_result() - see
// MariaDBStatement.hpp) rather than materialized up front. Query
// *execution* (mysql_real_query) happens eagerly at construction, same as
// SQLiteCursor's eager sqlite3_prepare_v2() - only row *fetching* is lazy.
//
// Unlike SQLiteCursor, this one's single-pass claim actually holds: SQLite
// silently restarts an exhausted statement the next time it's stepped (see
// SQLiteCursorTest::ReusingAnExhaustedCursorSilentlyReplaysTheWholeQuery),
// but there is no equivalent "rewind" for an unbuffered MYSQL_RES - once
// mysql_fetch_row() returns nullptr, it keeps returning nullptr. A second
// full iteration here yields zero rows rather than replaying the query.
class MariaDBCursor : public std::ranges::view_interface<MariaDBCursor> {
public:
  MariaDBCursor(MYSQL *connection, const String &sql)
      : connection(connection), result(run_unbuffered_query(connection, sql)) {}

  class iterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = Row;
    using difference_type = std::ptrdiff_t;

    iterator() = default;

    iterator(MYSQL *connection, ResultHandle result)
        : connection(connection), result(std::move(result)) {
      advance();
    }

    const Row &operator*() const { return current; }
    const Row *operator->() const { return &current; }

    iterator &operator++() {
      advance();
      return *this;
    }

    void operator++(int) { advance(); }

    bool operator==(std::default_sentinel_t) const { return done; }

  private:
    void advance() {
      if (result == nullptr) {
        done = true;
        return;
      }

      MYSQL_ROW row = mysql_fetch_row(result.get());

      if (row == nullptr) {
        done = true;

        if (mysql_errno(connection) != 0) {
          throw Core::Exceptions::RuntimeException("Failed to fetch row: {}",
                                                    mysql_error(connection));
        }

        return;
      }

      done = false;
      current.clear();

      unsigned int columns = mysql_num_fields(result.get());
      MYSQL_FIELD *fields = mysql_fetch_fields(result.get());

      for (unsigned int i = 0; i < columns; i++) {
        // Unlike SQLiteDriver::query()'s callback (see
        // SQLiteDriverTest::SelectingARowWithANullColumnThrowsOrCrashes_KnownCriticalDefect),
        // a NULL column is explicitly checked for here rather than handed
        // straight to String's constructor.
        current[fields[i].name] = row[i] != nullptr ? row[i] : "";
      }
    }

    MYSQL *connection = nullptr;
    ResultHandle result;
    Row current;
    bool done = true;
  };

  iterator begin() const { return iterator(connection, result); }
  std::default_sentinel_t end() const { return {}; }

private:
  MYSQL *connection = nullptr;
  ResultHandle result;
};

static_assert(std::ranges::input_range<MariaDBCursor>);
static_assert(std::ranges::view<MariaDBCursor>);

} // namespace CXORM::MariaDB
