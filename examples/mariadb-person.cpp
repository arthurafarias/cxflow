// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

// MariaDB counterpart to person.cpp: same QueryBuilder usage, same
// AbstractDriver::run() flow, only the driver and column-type enum differ.
// Unlike SQLite's "database.db" (created on demand, no server needed), this
// needs an actual MariaDB/MySQL server reachable at the host/port below,
// with a database already created:
//
//   CREATE DATABASE cxorm_example;
//
// Adjust host/user/password/database/port for your environment before
// running.

#include "CXORM/Core/SharedPointer.hpp"
#include "CXORM/Modules/SQL/Base/AbstractDriver.hpp"
#include <CXORM/Modules/SQL/Base/MariaDBDataType.hpp>
#include <CXORM/Modules/SQL/Base/QueryBuilder.hpp>
#include <CXORM/Modules/SQL/MariaDB/MariaDBDriver.hpp>

#include <format>

using namespace CXORM::Base;
using namespace CXORM::MariaDB;

int main(int argc, char *argv[]) {

  using namespace CXORM::MariaDB;
  SharedPointer<AbstractDriver> db =
      MariaDBDriver::create("127.0.0.1", "root", "", "cxorm_example", 3306);

  QueryBuilder::create()
    ->create_table("IF NOT EXISTS Person")
    ->fields_start()
    ->field("id", MariaDBDataType::Integer)
    ->field("name", MariaDBDataType::Text)
    ->field("age", MariaDBDataType::Integer)
    ->fields_end()
    ->run(db);

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
