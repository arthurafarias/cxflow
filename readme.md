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

# Contributing

We welcome issues and pull requests. Suggestions for optimizations, new drivers, or feature ideas are especially appreciated!

# License

This project is licensed under propertary license – see the [file](license.md) for details.