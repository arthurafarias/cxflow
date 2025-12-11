// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include <Modules/SQL/Base/QueryBuilder.hpp>
#include <Modules/SQL/SQLite/SQLiteDriver.hpp>

#include <format>

using namespace Modules::SQL::Base;
using namespace Modules::SQL::SQLite;

int main(int argc, char *argv[]) {
  auto db = SQLiteDriver("database.db");

  auto query = QueryBuilder::create()
                   ->select("*")
                   ->from("Person")
                   ->order_by("id")
                   ->limit("2");

  auto result = db.query(query);

  for (auto ptr : result) {
    auto row = *ptr;
    std::cout << std::format("ID: {}\t Name: {}\t\t Age: {}\n", row["id"].c_str(),
                             row["name"].c_str(), row["age"].c_str());
  }

  return 0;
}