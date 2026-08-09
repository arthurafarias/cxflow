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
#include <sqlite3.h>

using namespace CXORM::Core::Containers;

namespace CXORM::SQLite {

using StatementHandle = std::shared_ptr<sqlite3_stmt>;

inline StatementHandle prepare_statement(sqlite3 *db, const String &sql) {
  sqlite3_stmt *raw = nullptr;
  int retval = sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr);

  if (retval != SQLITE_OK) {
    throw Core::Exceptions::RuntimeException(
        "Failed to prepare statement '{}': {}", sql.c_str(),
        sqlite3_errmsg(db));
  }

  return StatementHandle(raw,
                         [](sqlite3_stmt *stmt) { sqlite3_finalize(stmt); });
}

} // namespace CXORM::SQLite
