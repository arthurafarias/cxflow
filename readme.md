# CXORM

![cxorm](misc/images/logo-banner-path.svg)

cxorm is an experimental ORM (Object Relational Model) that aims to simplify at minimum a ORM development using C++. Instead of using complex data mappings on objects. Here we rely on `Map<String,String>` where the first is the head and the second is the field.

The design is brand new and the project in an early conceptual stage. Many features are still under exploration.

The example below shows a query being built and executed by the driver. The result is presented in a collection of Maps. Memory usage wasn't taken into account, just usability for now. If you want to contribute with some optimizations over the algorithm. Feel free to contribute in the issues channel.

# Table of Contents

- [Usage](#Usage)
- [Contributing](#Contributing)
- [License](#License)

# Usage

Before starting, create a simple database in sqlite3 like the following:

```sql
.open database.db
CREATE TABLE Person(id INTEGER, name BLOB, age INTEGER);
INSERT INTO Person VALUES(0, 'Arthur', 35);
```

In a text file write following and then write in C++:

```c++
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
```

```bash
g++ -std=c++23 -Isrc examples/person.cpp -o cxorm_person_example -lsqlite3
```

After executing the code you will produce the following output

```
ID: 0    Name: Arthur de Araújo Farias           Age: 35
```

You also can use serialization module to match model directly using an Input or output archiver.

```
#include <Core/Logging/LoggerManager.hpp>

#include <Modules/Serialization/Base/AbstractArchiver.hpp>
#include <cassert>

#include <Modules/SQL/SQLite/SQLiteInputArchiver.hpp>
#include <Modules/SQL/SQLite/SQLiteOutputArchiver.hpp>

#include <sqlite3.h>
#include <string>

using namespace Modules::Serialization::Base;
using namespace Modules::SQL::SQLite;

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
```

It will fetch the last record from a model in a database and fill the model. You can compile this example by issuing:

```bash
g++ -std=c++23 -Isrc examples/person-direct-mapping.cpp -o cxorm_person_example -lsqlite3
```

It will print the following information

```
2025-12-11 11:33:18.167560400: INFO: INSERT INTO Person VALUES ( 0,'Arthur',37);
2025-12-11 11:33:18.174817223: INFO: SELECT * FROM Person LIMIT 1;
2025-12-11 11:33:18.175013242: INFO: 0 Arthur de Araújo Farias 35
```

It is shown that after inserting a value

# Contributing

We welcome issues and pull requests. Suggestions for optimizations, new drivers, or feature ideas are especially appreciated!

# License

This project is licensed under propertary license – see the [file](license.md) for details.