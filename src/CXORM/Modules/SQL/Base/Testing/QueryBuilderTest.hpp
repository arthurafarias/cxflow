// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <CXORM/Core/Containers/String.hpp>
#include <CXORM/Modules/SQL/Base/QueryBuilder.hpp>
#include <CXORM/Modules/SQL/Base/SQLiteDataType.hpp>
#include <CXORM/Testing/TestGroup.hpp>

namespace CXORM::Base::Testing {

inline static ::CXORM::Testing::TestGroup QueryBuilderTest{
    "QueryBuilder",
    {
        {"RunWithNullDriverReturnsNullResultInsteadOfThrowing",
         [](auto &ctx) {
           auto result =
               QueryBuilder::create()->select("*")->from("Person")->run(nullptr);
           ctx.check(result == nullptr, "run(nullptr) should return a null result");
         }},

        {"CompilesCreateTableWithFields",
         [](auto &ctx) {
           auto compiled = QueryBuilder::create()
                               ->create_table("IF NOT EXISTS Person")
                               ->fields_start()
                               ->field("id", SQLiteDataType::Integer)
                               ->field("name", SQLiteDataType::Text)
                               ->field("age", SQLiteDataType::Integer)
                               ->fields_end()
                               ->compile();
           ctx.check_equal(
               compiled,
               "CREATE TABLE IF NOT EXISTS Person (id INTEGER,name TEXT,age INTEGER);");
         }},

        {"CompilesSelectWhereOrderLimitOffsetChain",
         [](auto &ctx) {
           auto compiled = QueryBuilder::create()
                               ->select("*")
                               ->from("Person")
                               ->where("age > {}", 18)
                               ->order_by("id")
                               ->desc()
                               ->limit("{}", 2)
                               ->offset("{}", 5)
                               ->compile();
           ctx.check_equal(compiled, "SELECT * FROM Person WHERE age > 18 ORDER BY id "
                                     "DESC LIMIT 2 OFFSET 5;");
         }},

        // Note the stray space after the opening paren: values_start() and
        // values_end() are pushed as two separate tokens and compile() joins
        // all tokens with a plain space, so "VALUES (" and "1,'Ann',3.5)"
        // never fuse into "VALUES (1,...". Cosmetic (SQLite parses it fine
        // either way) but worth knowing if a test ever compares compiled
        // SQL verbatim.
        {"CompilesInsertValuesChainForMixedTypes",
         [](auto &ctx) {
           auto compiled = QueryBuilder::create()
                               ->insert()
                               ->into("Person")
                               ->values_start()
                               ->value(1)
                               ->value(String("Ann"))
                               ->value(3.5)
                               ->values_end()
                               ->compile();
           ctx.check_equal(compiled, "INSERT INTO Person VALUES ( 1,'Ann',3.5);");
         }},

        // KNOWN DEFECT (critical - see the SQL-injection proof in
        // ArchiverRoundTripTest.hpp): string values are wrapped in single
        // quotes but embedded quotes are never escaped/doubled, and there is
        // no bound-parameter API at all. Any string value containing a
        // single quote produces syntactically broken, and against a live
        // driver, exploitable SQL.
        {"StringValueWithEmbeddedQuoteProducesUnescapedBrokenSql",
         [](auto &ctx) {
           auto compiled = QueryBuilder::create()
                               ->insert()
                               ->into("Person")
                               ->values_start()
                               ->value(String("O'Brien"))
                               ->values_end()
                               ->compile();
           ctx.check_equal(compiled, "INSERT INTO Person VALUES ( 'O'Brien');");
         }},

        // KNOWN DEFECT: fields_start() is a no-op - it does not clear the
        // `fields` collection - so field()/fields_end() calls on a *reused*
        // QueryBuilder instance silently accumulate fields from every
        // previous fields_end() call instead of starting a fresh column
        // list.
        {"ReusingBuilderAcrossFieldsBlocksAccumulatesFields_KnownDefect",
         [](auto &ctx) {
           auto qb = QueryBuilder::create();
           qb->fields_start()->field("a", SQLiteDataType::Integer)->fields_end();
           qb->fields_start()->field("b", SQLiteDataType::Integer)->fields_end();
           ctx.check_equal(qb->compile(), "(a INTEGER) (a INTEGER,b INTEGER);");
         }},

        // SQLiteDataType::Null and ::Blob are declared but never exercised
        // by the examples (only Integer/Text/Real show up there).
        {"CreateTableAcceptsNullAndBlobColumnTypes",
         [](auto &ctx) {
           auto compiled = QueryBuilder::create()
                               ->create_table("Attachment")
                               ->fields_start()
                               ->field("placeholder", SQLiteDataType::Null)
                               ->field("payload", SQLiteDataType::Blob)
                               ->fields_end()
                               ->compile();
           ctx.check_equal(compiled,
                           "CREATE TABLE Attachment (placeholder NULL,payload BLOB);");
         }},
    }};

} // namespace CXORM::Base::Testing
