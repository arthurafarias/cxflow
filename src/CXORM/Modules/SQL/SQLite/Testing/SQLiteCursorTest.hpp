// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <CXORM/Core/Exceptions/RuntimeException.hpp>
#include <CXORM/Modules/SQL/Base/QueryBuilder.hpp>
#include <CXORM/Modules/SQL/SQLite/SQLiteCursor.hpp>
#include <CXORM/Modules/SQL/SQLite/SQLiteDriver.hpp>
#include <CXORM/Modules/SQL/SQLite/Testing/TestSupport.hpp>
#include <CXORM/Testing/TestGroup.hpp>

#include <ranges>
#include <stdexcept>

namespace CXORM::SQLite::Testing {

inline static ::CXORM::Testing::TestGroup SQLiteCursorTest{
    "SQLiteCursor",
    {
        {"IteratesAllRowsInInsertionOrderWithNoOrderBy",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-cursor-insertion-order.db");
           SQLiteDriver db{String(file.path)};
           seed_person_table_with_rows(db);

           Collection<String> names;
           for (const auto &row :
                CXORM::Base::QueryBuilder::create()->select("*")->from("Person")->cursor(db)) {
             names.push_back(row.at("name"));
           }

           if (!ctx.require_equal(names.size(), 5u, "names.size()")) {
             return;
           }
           ctx.check_equal(names[0], "Ann");
           ctx.check_equal(names[4], "Ed");
         }},

        {"ComposesWithFilterTransformAndTakeLikeTheExample",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-cursor-filter-transform-take.db");
           SQLiteDriver db{String(file.path)};
           seed_person_table_with_rows(db);

           auto adults =
               CXORM::Base::QueryBuilder::create()->select("*")->from("Person")->cursor(db) |
               std::views::filter(
                   [](const Row &row) { return std::stoi(row.at("age")) >= 18; }) |
               std::views::transform([](const Row &row) { return row.at("name"); }) |
               std::views::take(2);

           Collection<String> names;
           for (auto &&name : adults) {
             names.push_back(name);
           }

           if (!ctx.require_equal(names.size(), 2u, "names.size()")) {
             return;
           }
           ctx.check_equal(names[0], "Ann");
           ctx.check_equal(names[1], "Cy");
         }},

        {"ChunkGroupsRowsIntoFixedSizePagesIncludingTheLastPartialOne",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-cursor-chunk-paging.db");
           SQLiteDriver db{String(file.path)};
           seed_person_table_with_rows(db);

           auto pages =
               CXORM::Base::QueryBuilder::create()->select("*")->from("Person")->cursor(db) |
               std::views::chunk(2);

           size_t page_count = 0;
           size_t last_page_size = 0;
           for (auto page : pages) {
             ++page_count;
             last_page_size = std::ranges::distance(page);
           }

           ctx.check_equal(page_count, 3u); // 5 rows -> pages of 2, 2, 1
           ctx.check_equal(last_page_size, 1u);
         }},

        // SURPRISE (contradicts the class's own doc comment): SQLiteCursor
        // is documented as "single-pass: intended to be iterated once, as
        // with std::istream_iterator", implying a second traversal should
        // come back empty. It does not. begin() always wraps the SAME
        // underlying prepared statement, and this build of SQLite implicitly
        // resets a statement the next time sqlite3_step() is called after
        // it already returned SQLITE_DONE (confirmed independently against
        // the system libsqlite3 via a standalone C reproduction) - so a
        // second full iteration silently re-runs the entire query and
        // replays every row again. Consuming a cursor twice by accident
        // (e.g. once to log a count, again to actually use the rows) burns
        // a second round-trip instead of failing loudly or coming back
        // empty, and for a statement with side effects it would repeat them.
        {"ReusingAnExhaustedCursorSilentlyReplaysTheWholeQuery",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-cursor-replay.db");
           SQLiteDriver db{String(file.path)};
           seed_person_table_with_rows(db);

           auto cursor =
               CXORM::Base::QueryBuilder::create()->select("*")->from("Person")->cursor(db);

           size_t first_pass_count = std::ranges::distance(cursor);
           size_t second_pass_count = std::ranges::distance(cursor);

           ctx.check_equal(first_pass_count, 5u);
           ctx.check_equal(second_pass_count, 5u);
         }},

        // The class comment describes cursor() as "lazy" - and row
        // *fetching* is - but statement *preparation* is not: invalid SQL
        // is rejected the moment cursor() is called, before a single row
        // would ever be requested.
        {"InvalidSqlThrowsImmediatelyAtCursorCreationNotAtIteration",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-cursor-invalid-sql.db");
           SQLiteDriver db{String(file.path)};
           seed_person_table_with_rows(db);

           ctx.template check_throws<Core::Exceptions::RuntimeException>(
               [&] {
                 auto cursor = CXORM::Base::QueryBuilder::create()
                                   ->select("*")
                                   ->from("NoSuchTable")
                                   ->cursor(db);
                 (void)cursor;
               },
               "cursor() over NoSuchTable");
         }},

        // Unlike the eager query()/run() path (see
        // SQLiteDriverTest::SelectingARowWithANullColumnThrowsOrCrashes_KnownCriticalDefect),
        // the cursor path does NOT crash on NULL columns -
        // sqlite3_column_text() returns nullptr for a NULL value and the
        // cursor substitutes "". That avoids the crash but silently
        // conflates SQL NULL with an empty string: callers cannot tell "no
        // name was recorded" from "name was set to the empty string" - and
        // consumers like the filter predicate in person-direct-mapping.cpp
        // (`std::stoi(row.at("age"))`) will still throw
        // std::invalid_argument the moment they touch a NULL numeric
        // column.
        {"NullColumnBecomesEmptyStringAndBreaksNumericParsing",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-cursor-null-column.db");
           SQLiteDriver db{String(file.path)};
           seed_person_table_with_rows(db);
           db.query(CXORM::Base::QueryBuilder::create()
                         ->insert()
                         ->into("Person")
                         ->append_tag("(id, name)")
                         ->values_start()
                         ->value(6)
                         ->value(String("NoAge"))
                         ->values_end());

           auto cursor = CXORM::Base::QueryBuilder::create()
                             ->select("*")
                             ->from("Person")
                             ->where("id = 6")
                             ->cursor(db);
           auto it = cursor.begin();
           if (!ctx.check(it != cursor.end(), "expected at least one row")) {
             return;
           }
           ctx.check_equal((*it).at("age"), "");
           ctx.template check_throws<std::invalid_argument>(
               [&] { std::stoi((*it).at("age")); }, "std::stoi(NULL age)");
         }},

        // iterator::advance() throws when sqlite3_step() returns anything
        // other than SQLITE_ROW or SQLITE_DONE. That path needs a
        // *step-time* SQLite error, as opposed to a prepare-time one
        // (already covered above): prepare a SELECT, then invalidate its
        // schema out from under it - on the SAME connection, via a second
        // statement, before the first row is ever fetched - so the
        // already-compiled statement fails when it is finally stepped
        // rather than when it was prepared.
        {"StepTimeSqlErrorThrowsFromTheIterator",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-cursor-step-time-error.db");
           SQLiteDriver db{String(file.path)};
           seed_person_table_with_rows(db);

           auto cursor =
               CXORM::Base::QueryBuilder::create()->select("name")->from("Person")->cursor(db);

           db.query(CXORM::Base::QueryBuilder::create()->append_tag(
               "ALTER TABLE Person DROP COLUMN name"));

           ctx.template check_throws<Core::Exceptions::RuntimeException>(
               [&] { (void)*cursor.begin(); }, "step after schema invalidated");
         }},
    }};

} // namespace CXORM::SQLite::Testing
