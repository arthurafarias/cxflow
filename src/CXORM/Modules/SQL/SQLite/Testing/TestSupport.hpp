// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// Shared helpers for every *.hpp under this Testing/ directory. Not itself a
// class's test facility (it declares no TestGroup), but it lives here
// because every one of these fixtures is SQLite-specific, and the generated
// test binary just #includes everything it finds under a Testing/
// directory regardless of whether that file registers any cases.

#include <CXORM/Core/Containers/String.hpp>
#include <CXORM/Modules/SQL/Base/QueryBuilder.hpp>
#include <CXORM/Modules/SQL/SQLite/SQLiteDriver.hpp>

#include <filesystem>
#include <tuple>

namespace CXORM::SQLite::Testing {

// RAII: guarantees a fresh, uniquely-named SQLite file for a test case and
// removes it both before (in case a previous run crashed mid-test) and
// after.
struct TempDbFile {
  Core::Containers::String path;

  explicit TempDbFile(const char *name) : path(name) {
    std::filesystem::remove(path.c_str());
  }

  ~TempDbFile() { std::filesystem::remove(path.c_str()); }
};

inline void create_person_table(SQLiteDriver &db,
                                const char *ddl = "IF NOT EXISTS Person") {
  using namespace CXORM::Base;
  db.query(QueryBuilder::create()
                ->create_table("{}", ddl)
                ->fields_start()
                ->field("id", SQLiteDataType::Integer)
                ->field("name", SQLiteDataType::Text)
                ->field("age", SQLiteDataType::Integer)
                ->fields_end());
}

inline void seed_person_table_with_rows(SQLiteDriver &db) {
  using namespace CXORM::Base;
  create_person_table(db);

  for (auto [id, name, age] :
       {std::tuple{1, "Ann", 29}, std::tuple{2, "Bo", 15}, std::tuple{3, "Cy", 41},
        std::tuple{4, "Di", 17}, std::tuple{5, "Ed", 33}}) {
    db.query(QueryBuilder::create()
                  ->insert()
                  ->into("Person")
                  ->values_start()
                  ->value(id)
                  ->value(Core::Containers::String(name))
                  ->value(age)
                  ->values_end());
  }
}

} // namespace CXORM::SQLite::Testing
