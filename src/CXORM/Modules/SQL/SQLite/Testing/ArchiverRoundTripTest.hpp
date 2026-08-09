// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <CXORM/Core/Exceptions/RuntimeException.hpp>
#include <CXORM/Modules/SQL/SQLite/SQLiteInputArchiver.hpp>
#include <CXORM/Modules/SQL/SQLite/SQLiteOutputArchiver.hpp>
#include <CXORM/Modules/SQL/SQLite/Testing/TestSupport.hpp>
#include <CXORM/Modules/Serialization/Base/AbstractArchiver.hpp>
#include <CXORM/Testing/TestGroup.hpp>

namespace CXORM::SQLite::Testing {

using namespace CXORM::Serialization::Base;

// Mirrors examples/person-direct-mapping.cpp exactly: same struct, same
// operator%, same field order.
struct Person {
  int id;
  String name;
  int age;
};

template <typename Archiver> Archiver &operator%(Archiver &ar, Person &person) {
  ar % ArchiveTagFactory::make_object_start("Person");
  ar % ArchiveTagFactory::make_named_value_property("id", person.id);
  ar % ArchiveTagFactory::make_named_value_property("name", person.name);
  ar % ArchiveTagFactory::make_named_value_property("age", person.age);
  ar % ArchiveTagFactory::make_object_end("Person");
  return ar;
}

// A second model exercising the double-valued KeyValueTag overload, which
// Person never touches.
struct Reading {
  int id;
  double celsius;
};

template <typename Archiver> Archiver &operator%(Archiver &ar, Reading &reading) {
  ar % ArchiveTagFactory::make_object_start("Reading");
  ar % ArchiveTagFactory::make_named_value_property("id", reading.id);
  ar % ArchiveTagFactory::make_named_value_property("celsius", reading.celsius);
  ar % ArchiveTagFactory::make_object_end("Reading");
  return ar;
}

namespace {

void create_person_table_via_archiver(const String &path) {
  auto output = SQLiteOutputArchiver(path);
  output.query(CXORM::Base::QueryBuilder::create()
                   ->create_table("IF NOT EXISTS Person")
                   ->fields_start()
                   ->field("id", SQLiteDataType::Integer)
                   ->field("name", SQLiteDataType::Text)
                   ->field("age", SQLiteDataType::Integer)
                   ->fields_end());
}

void create_reading_table_via_archiver(const String &path) {
  auto output = SQLiteOutputArchiver(path);
  output.query(CXORM::Base::QueryBuilder::create()
                   ->create_table("IF NOT EXISTS Reading")
                   ->fields_start()
                   ->field("id", SQLiteDataType::Integer)
                   ->field("celsius", SQLiteDataType::Real)
                   ->fields_end());
}

} // namespace

inline static ::CXORM::Testing::TestGroup ArchiverRoundTripTest{
    "ArchiverRoundTrip",
    {
        {"WritesThenReadsBackTheSameFields",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-archiver-basic-roundtrip.db");
           create_person_table_via_archiver(file.path);

           {
             auto output = SQLiteOutputArchiver(file.path);
             Person person{.id = 1, .name = "Arthur", .age = 36};
             output % person;
           }

           auto input = SQLiteInputArchiver(file.path);
           Person person{};
           input % person;

           ctx.check_equal(person.id, 1);
           ctx.check_equal(person.name, "Arthur");
           ctx.check_equal(person.age, 36);
         }},

        {"InputArchiverFetchesHighestIdWhenIdsAreDistinct",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-archiver-highest-id.db");
           create_person_table_via_archiver(file.path);

           {
             auto output = SQLiteOutputArchiver(file.path);
             Person first{.id = 1, .name = "First", .age = 20};
             Person second{.id = 2, .name = "Second", .age = 21};
             output % first;
             output % second;
           }

           auto input = SQLiteInputArchiver(file.path);
           Person person{};
           input % person;

           ctx.check_equal(person.name, "Second");
         }},

        {"RoundTripsADoubleValuedField",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-archiver-double-field.db");
           create_reading_table_via_archiver(file.path);

           {
             auto output = SQLiteOutputArchiver(file.path);
             Reading reading{.id = 1, .celsius = 36.6};
             output % reading;
           }

           auto input = SQLiteInputArchiver(file.path);
           Reading reading{};
           input % reading;

           ctx.check_equal(reading.celsius, 36.6);
         }},

        // KNOWN DEFECT: the archiver has no concept of an auto-increment/
        // "skip on insert" column. The example table is declared as a plain
        // `id INTEGER` (not `INTEGER PRIMARY KEY`), and operator% always
        // serializes every field including `id` verbatim - so nothing in
        // the library stops two records from being written with the same
        // id. When that happens, "ORDER BY id DESC LIMIT 1" (what
        // SQLiteInputArchiver uses internally to find "the" record) can no
        // longer identify a most-recently-written row: ties are broken by
        // SQLite's internal row order, not insertion order. This only
        // asserts the safely-observable half of that (both rows really do
        // land with the same id) rather than which one comes back, because
        // the tie-break itself is unspecified.
        {"DuplicateIdsAreAllowed_KnownDesignGap",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-archiver-duplicate-ids.db");
           create_person_table_via_archiver(file.path);

           {
             auto output = SQLiteOutputArchiver(file.path);
             Person first{.id = 0, .name = "First", .age = 20};
             Person second{.id = 0, .name = "Second", .age = 21};
             output % first;
             output % second;
           }

           auto db = SQLiteDriver::create(file.path);
           auto result = CXORM::Base::QueryBuilder::create()
                             ->select("count(*)")
                             ->from("Person")
                             ->where("id = 0")
                             ->run(db);
           if (!ctx.require_equal(result->size(), 1u, "result->size()")) {
             return;
           }
           ctx.check_equal((*result->front())["count(*)"], "2");
         }},

        // KNOWN DEFECT (crashes the process): SQLiteInputArchiver's handler
        // for ObjectTag::Start unconditionally does
        // `ar.result = ar.query(ar.expression)->front();` - Collection::
        // front() is std::deque::front(), which is undefined behavior on an
        // empty deque. Any attempt to read a model out of a table that has
        // zero matching rows (a fresh table, or a WHERE-less fetch after
        // every row was deleted) crashes instead of raising a catchable
        // "not found" condition. Run isolated + expect_crash: this is
        // genuine UB, not a catchable exception, so it must not take the
        // whole test binary down with it.
        ::CXORM::Testing::TestCase{
            "ReadingFromAnEmptyTableCrashes_KnownCriticalDefect",
            [](auto &ctx) {
              TempDbFile file("cxorm-test-archiver-empty-table-crash.db");
              create_person_table_via_archiver(file.path);

              auto input = SQLiteInputArchiver(file.path);
              Person person{};
              input % person; // expected to crash before returning
              ctx.check(false, "should have crashed before reaching this point");
            }}
            .expect_crash(),

        // CRITICAL SECURITY DEFECT: QueryBuilder::value(const String&)
        // wraps the value in single quotes without escaping embedded
        // quotes, and the output archiver builds the final INSERT via that
        // string-concatenation path with no parameter binding anywhere. A
        // String field under attacker control can therefore terminate the
        // INSERT statement and inject arbitrary additional SQL, which
        // sqlite3_exec happily runs as a second statement in the same
        // script. This proves it end-to-end: serializing a single Person
        // whose `name` carries a crafted payload drops the Person table
        // outright. This is the single highest-priority requirement this
        // exploration surfaced: the library needs bound parameters
        // (sqlite3_bind_*) before it is safe to use with any value that
        // isn't a compile-time literal.
        {"OutputArchiverStringFieldAllowsSqlInjection_CriticalDefect",
         [](auto &ctx) {
           TempDbFile file("cxorm-test-archiver-sql-injection.db");
           create_person_table_via_archiver(file.path);

           {
             auto output = SQLiteOutputArchiver(file.path);
             Person malicious{
                 .id = 1, .name = "z',0); DROP TABLE Person; --", .age = 99};
             output % malicious;
           }

           auto db = SQLiteDriver::create(file.path);
           ctx.template check_throws<Core::Exceptions::RuntimeException>(
               [&] {
                 CXORM::Base::QueryBuilder::create()->select("*")->from("Person")->run(db);
               },
               "SELECT * FROM Person after injected DROP TABLE");
         }},
    }};

} // namespace CXORM::SQLite::Testing
