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
#include <CXORM/Modules/SQL/SQLite/SQLiteDriver.hpp>
#include <CXORM/Modules/SQL/SQLite/Testing/TestSupport.hpp>
#include <CXORM/Testing/TestGroup.hpp>

#include <exception>
#include <filesystem>

namespace CXORM::SQLite::Testing {

inline static ::CXORM::Testing::TestGroup SQLiteDriverTest{
    "SQLiteDriver",
    {
        {"CreateInsertSelectRoundTripsThroughEagerQuery",
         [](auto &ctx) {
           using namespace CXORM::Base;
           TempDbFile file("cxorm-test-driver-round-trip.db");
           SQLiteDriver db{String(file.path)};
           create_person_table(db);

           db.query(QueryBuilder::create()
                         ->insert()
                         ->into("Person")
                         ->values_start()
                         ->value(1)
                         ->value(String("Ann"))
                         ->value(29)
                         ->values_end());

           auto result = db.query(QueryBuilder::create()->select("*")->from("Person"));

           if (!ctx.require_equal(result->size(), 1u, "result->size()")) {
             return;
           }
           ctx.check_equal((*result->front())["id"], "1");
           ctx.check_equal((*result->front())["name"], "Ann");
           ctx.check_equal((*result->front())["age"], "29");
         }},

        {"SelectOnEmptyTableReturnsEmptyResultNotAnError",
         [](auto &ctx) {
           using namespace CXORM::Base;
           TempDbFile file("cxorm-test-driver-empty-select.db");
           SQLiteDriver db{String(file.path)};
           create_person_table(db);

           auto result = db.query(QueryBuilder::create()->select("*")->from("Person"));
           ctx.check_equal(result->size(), 0u);
         }},

        {"CreateTableIfNotExistsIsIdempotent",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-driver-idempotent-create.db");
           SQLiteDriver db{String(file.path)};
           create_person_table(db);

           bool threw = false;
           try {
             create_person_table(db);
           } catch (const std::exception &) {
             threw = true;
           }
           ctx.check(!threw, "calling create_person_table twice should not throw");
         }},

        {"CreateTableWithoutIfNotExistsOnExistingTableThrows",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-driver-create-without-ifnotexists.db");
           SQLiteDriver db{String(file.path)};
           create_person_table(db, "IF NOT EXISTS Person");

           ctx.template check_throws<Core::Exceptions::RuntimeException>(
               [&] { create_person_table(db, "Person"); },
               "create_person_table(db, \"Person\")");
         }},

        {"SelectFromMissingTableThrows",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-driver-select-missing-table.db");
           SQLiteDriver db{String(file.path)};

           ctx.template check_throws<Core::Exceptions::RuntimeException>(
               [&] {
                 db.query(
                     CXORM::Base::QueryBuilder::create()->select("*")->from("NoSuchTable"));
               },
               "select from NoSuchTable");
         }},

        // sqlite3_open() cannot turn a directory into a database file. This
        // is a reliable, privilege-independent way to force
        // SQLITE_CANTOPEN, unlike e.g. a permission-denied path (which
        // succeeds if the suite ever runs as root).
        {"OpeningAPathThatCannotBeCreatedAsAFileThrows",
         [](auto &ctx) {
           auto dir =
               std::filesystem::temp_directory_path() / "cxorm-test-unopenable-dir.db";
           std::filesystem::create_directory(dir);

           ctx.template check_throws<Core::Exceptions::RuntimeException>(
               [&] { SQLiteDriver::create(Core::Containers::String(dir.string())); },
               "SQLiteDriver::create(directory path)");

           std::filesystem::remove_all(dir);
         }},

        // KNOWN CRITICAL DEFECT: sqlite3_exec()'s callback passes a NULL
        // `char*` for any column whose value is SQL NULL. SQLiteDriver::
        // query()'s callback assigns that pointer straight into a String
        // via std::string::operator=(const char*) with no null check -
        // constructing a std::string from a null pointer is undefined
        // behavior per the standard. On this toolchain (hardened
        // libstdc++) that UB happens to manifest as a thrown
        // std::logic_error ("basic_string: construction from null is not
        // valid") rather than a silent segfault - which is why this is
        // asserted as a throw rather than run as an isolated/expect_crash
        // case. That is still a real defect: nothing in
        // SQLiteDriver::query() catches or anticipates this, so a plain
        // SELECT throws an unhandled exception the moment any column
        // happens to be NULL, and on a libstdc++ without the hardening
        // checks (or a different standard library entirely) the same code
        // path is a plain crash instead. Either way, `values[i]` needs a
        // null check before it reaches String's constructor.
        {"SelectingARowWithANullColumnThrowsOrCrashes_KnownCriticalDefect",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-driver-null-column.db");
           SQLiteDriver db{String(file.path)};
           create_person_table(db);

           db.query(CXORM::Base::QueryBuilder::create()
                         ->insert()
                         ->into("Person")
                         ->append_tag("(id, age)")
                         ->values_start()
                         ->value(1)
                         ->value(30)
                         ->values_end());

           ctx.template check_throws<std::exception>(
               [&] {
                 db.query(CXORM::Base::QueryBuilder::create()->select("*")->from("Person"));
               },
               "select with a NULL column");
         }},
    }};

} // namespace CXORM::SQLite::Testing
