// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include "CXORM/Modules/SQL/Base/QueryBuilder.hpp"
#include <CXORM/Core/Logging/LoggerManager.hpp>

#include <CXORM/Modules/Serialization/Base/AbstractArchiver.hpp>
#include <cassert>

#include <CXORM/Modules/SQL/SQLite/SQLiteInputArchiver.hpp>
#include <CXORM/Modules/SQL/SQLite/SQLiteOutputArchiver.hpp>

#include <exception>
#include <iostream>
#include <ranges>
#include <sqlite3.h>
#include <string>

using namespace CXORM::Serialization::Base;
using namespace CXORM::SQLite;
using namespace CXORM;

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

int main(int argc, char *argv[]) {

  Core::Logging::LoggerManager::level_set(
      Core::Logging::LoggerManager::Level::Debug);
  Core::Logging::LoggerManager::stream_set(
      Core::Logging::LoggerManager::stream_cout());
  {
    auto output = SQLiteOutputArchiver("database.db");
    try {
      output.query(QueryBuilder::create()
                       ->create_table("IF NOT EXISTS Person")
                       ->fields_start()
                       ->field("id", SQLiteDataType::Integer)
                       ->field("name", SQLiteDataType::Text)
                       ->field("age", SQLiteDataType::Integer)
                       ->fields_end());

      // Lazily stream rows from SQLite (one sqlite3_step at a time) and
      // compose the result with standard range adaptors: filter, transform,
      // and take, exactly as you would with any other C++ range.
      auto adults =
          QueryBuilder::create()->select("*")->from("Person")->cursor(output)
          | std::views::filter([](const Row &row) {
              return std::stoi(row.at("age")) >= 18;
            })
          | std::views::transform([](const Row &row) {
              return std::format("id: {}, name: {}", row.at("id").c_str(),
                                 row.at("name").c_str());
            })
          | std::views::take(10);

      for (const auto &line : adults) {
        std::cout << line << "\n";
      }

      // Paging: std::views::chunk groups the same lazy row stream into
      // fixed-size pages without ever materializing the full result set.
      auto pages =
          QueryBuilder::create()->select("*")->from("Person")->cursor(output)
          | std::views::chunk(2);

      size_t page_number = 0;
      for (auto page : pages) {
        std::cout << std::format("-- page {} --\n", ++page_number);
        for (const auto &row : page) {
          std::cout << std::format("id: {}, name: {}\n", row.at("id").c_str(),
                                   row.at("name").c_str());
        }
      }

    } catch (std::exception &e) {
      Core::Logging::LoggerManager::error("Error creating table: {}", e.what());
    }
  }

  {
    auto output = SQLiteOutputArchiver("database.db");
    auto person = Person();
    person.name = "Arthur";
    person.age = 36;
    output % person;
  }

  {
    auto input = SQLiteInputArchiver("database.db");
    auto person = Person();
    input % person;
    Core::Logging::LoggerManager::info("{} {} {}", person.id,
                                       person.name.c_str(), person.age);
  }

  return 0;
}