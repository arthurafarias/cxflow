// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include "CXORM/Modules/SQL/MariaDB/MariaDBDriver.hpp"
#include "CXORM/Modules/Serialization/Base/KeyValueTag.hpp"
#include "CXORM/Modules/Serialization/Base/ObjectTag.hpp"

using namespace CXORM::Serialization::Base;
using namespace CXORM::Base;

namespace CXORM::MariaDB {

namespace {
template <typename ValueType> ValueType from_string(const String &value);
template <> String from_string(const String &value) { return value; }
template <> int from_string(const String &value) {
  return std::stoi(value.c_str());
}
template <> double from_string(const String &value) {
  return std::stod(value.c_str());
}
} // namespace

class MariaDBInputArchiver : public MariaDBDriver {
public:
  using MariaDBDriver::MariaDBDriver;
  SharedPointer<QueryBuilder> expression;
  Collection<std::function<void()>> callbacks;
  SharedPointer<Map<String, String>> result;

private:
};

MariaDBInputArchiver constexpr &operator%(MariaDBInputArchiver &ar,
                                          const SharedPointer<ObjectTag> &tag) {

  if (tag->part == TagPart::Start) {
    ar.expression = QueryBuilder::create()
                        ->select("*")
                        ->from("{}", tag->name.c_str())
                        ->order_by("id")
                        ->desc()
                        ->limit("1");
    // Deliberately mirrors SQLiteInputArchiver's ObjectTag::Start handler
    // exactly, including its known critical defect: front() on a
    // zero-length result (nothing matched the SELECT) is undefined
    // behavior. Not fixed independently here - it's a shared serialization-
    // protocol issue (see ArchiverRoundTripTest::
    // ReadingFromAnEmptyTableCrashes_KnownCriticalDefect on the SQLite
    // side), and fixing it in only one backend would leave the two archivers
    // behaving differently for the same mistake.
    ar.result = ar.query(ar.expression)->front();
  }

  if (tag->part == TagPart::End) {
    for (auto fn : ar.callbacks) {
      fn();
    }
  }

  return ar;
}

MariaDBInputArchiver constexpr &
operator%(MariaDBInputArchiver &ar, SharedPointer<KeyValueTag<double>> tag) {
  ar.callbacks.push_back([&ar, tag]() {
    double value = from_string<double>((*ar.result)[tag->name]);
    (*tag->value) = value;
  });
  return ar;
}

MariaDBInputArchiver constexpr &operator%(MariaDBInputArchiver &ar,
                                          SharedPointer<KeyValueTag<int>> tag) {
  ar.callbacks.push_back([&ar, tag]() {
    int value = from_string<int>((*ar.result)[tag->name]);
    *(tag->value) = value;
  });
  return ar;
}

MariaDBInputArchiver constexpr &
operator%(MariaDBInputArchiver &ar, SharedPointer<KeyValueTag<String>> tag) {
  ar.callbacks.push_back([&ar, tag]() {
    String value = (*ar.result)[tag->name];
    *(tag->value) = value;
  });
  return ar;
}

} // namespace CXORM::MariaDB
