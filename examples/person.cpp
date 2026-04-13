// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include "Core/SharedPointer.hpp"
#include "Modules/SQL/Base/AbstractDriver.hpp"
#include <Modules/SQL/Base/QueryBuilder.hpp>
#include <Modules/SQL/Base/SQLiteDataType.hpp>
#include <Modules/SQL/SQLite/SQLiteDriver.hpp>

#include <format>

using namespace CXORM::Base;
using namespace CXORM::SQLite;

int main(int argc, char *argv[]) {

  using namespace CXORM::SQLite;
  SharedPointer<AbstractDriver> db = SQLiteDriver::create("database.db");

  QueryBuilder::create()
    ->create_table("Person")
    ->fields_start()
    ->field("id", SQLiteDataType::Integer)
    ->field("name", SQLiteDataType::Text)
    ->field("age", SQLiteDataType::Integer)
    ->fields_end()
    ;

  auto result = QueryBuilder::create()
                   ->select("*")
                   ->from("Person")
                   ->order_by("id")
                   ->limit("2")
                   ->run(db);

  for (auto ptr : *result) {
    auto row = *ptr;
    std::cout << std::format("ID: {}\t Name: {}\t\t Age: {}\n", row["id"].c_str(),
                             row["name"].c_str(), row["age"].c_str());
  }

  return 0;
}