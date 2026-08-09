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
#include "CXORM/Modules/SQL/SQLite/SQLiteStatement.hpp"

#include <iterator>
#include <ranges>
#include <sqlite3.h>
#include <utility>

using namespace CXORM::Core::Containers;

namespace CXORM::SQLite {

// A single result row, keyed by column name. Same shape as the eager
// QueryResult rows so code moving between `query()` and `cursor()` doesn't
// need to learn a second row type.
using Row = Map<String, String>;

// A lazy, single-pass, standards-compliant view over a SELECT's result set.
// Rows are pulled from SQLite one `sqlite3_step` at a time as the range is
// iterated, so results never have to be materialized in full up front. Being
// a real `std::ranges::view` means filtering, transforming and paging are
// not bespoke methods here - they are `std::views::filter`, `::transform`,
// `::take`, `::drop`, `::chunk`, and friends, applied directly to the
// cursor.
class SQLiteCursor : public std::ranges::view_interface<SQLiteCursor> {
public:
  SQLiteCursor(sqlite3 *db, const String &sql)
      : db(db), statement(prepare_statement(db, sql)) {}

  class iterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = Row;
    using difference_type = std::ptrdiff_t;

    iterator() = default;

    iterator(sqlite3 *db, StatementHandle statement)
        : db(db), statement(std::move(statement)) {
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
      int retval = sqlite3_step(statement.get());

      if (retval == SQLITE_ROW) {
        done = false;
        current.clear();
        int columns = sqlite3_column_count(statement.get());

        for (int i = 0; i < columns; i++) {
          const char *name = sqlite3_column_name(statement.get(), i);
          const unsigned char *text = sqlite3_column_text(statement.get(), i);
          current[name] =
              text != nullptr ? reinterpret_cast<const char *>(text) : "";
        }

        return;
      }

      done = true;

      if (retval != SQLITE_DONE) {
        throw Core::Exceptions::RuntimeException("Failed to step query: {}",
                                                  sqlite3_errmsg(db));
      }
    }

    sqlite3 *db = nullptr;
    StatementHandle statement;
    Row current;
    bool done = true;
  };

  // Single-pass: intended to be iterated once, as with std::istream_iterator.
  iterator begin() const { return iterator(db, statement); }
  std::default_sentinel_t end() const { return {}; }

private:
  sqlite3 *db = nullptr;
  StatementHandle statement;
};

static_assert(std::ranges::input_range<SQLiteCursor>);
static_assert(std::ranges::view<SQLiteCursor>);

} // namespace CXORM::SQLite
