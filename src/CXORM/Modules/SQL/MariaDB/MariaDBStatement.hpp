// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include "CXORM/Core/Containers/String.hpp"
#include "CXORM/Core/Exceptions/RuntimeException.hpp"

#include <memory>
#include <mysql.h>

using namespace CXORM::Core::Containers;

namespace CXORM::MariaDB {

// MariaDB's C API has no direct analogue of sqlite3's prepared-statement
// row-by-row stepping for plain text queries; the closest equivalent is
// mysql_use_result(), which streams rows from the server one at a time via
// repeated mysql_fetch_row() calls instead of buffering the whole result
// set up front the way mysql_store_result() does. ResultHandle wraps that
// the same way SQLite's StatementHandle wraps a sqlite3_stmt*.
using ResultHandle = std::shared_ptr<MYSQL_RES>;

// Runs `sql` and returns its (unbuffered) result set, or nullptr for
// statements that don't produce one (INSERT/UPDATE/CREATE/...). Throws on
// any actual failure - both a failed mysql_real_query() and a failed
// mysql_use_result() on a statement that DID promise a result set (per
// mysql_field_count()) are treated as errors; a null result together with
// zero expected fields is the normal, successful "no rows to stream" case.
inline ResultHandle run_unbuffered_query(MYSQL *connection, const String &sql) {
  if (mysql_real_query(connection, sql.c_str(), sql.length()) != 0) {
    throw Core::Exceptions::RuntimeException("Failed to execute query '{}': {}",
                                             sql.c_str(), mysql_error(connection));
  }

  if (mysql_field_count(connection) == 0) {
    return nullptr;
  }

  MYSQL_RES *raw = mysql_use_result(connection);

  if (raw == nullptr) {
    throw Core::Exceptions::RuntimeException("Failed to fetch result for '{}': {}",
                                             sql.c_str(), mysql_error(connection));
  }

  return ResultHandle(raw, [](MYSQL_RES *result) { mysql_free_result(result); });
}

} // namespace CXORM::MariaDB
