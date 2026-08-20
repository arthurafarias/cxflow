// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include "CXORM/Modules/SQL/Base/QueryBuilder.hpp"
#include "CXORM/Modules/SQL/MariaDB/MariaDBDriver.hpp"
#include "CXORM/Modules/Serialization/Base/KeyValueTag.hpp"
#include "CXORM/Modules/Serialization/Base/ObjectTag.hpp"

using namespace CXORM::Serialization::Base;
using namespace CXORM::Base;

namespace CXORM::MariaDB {
class MariaDBOutputArchiver : public MariaDBDriver {
public:
  using MariaDBDriver::MariaDBDriver;
  SharedPointer<QueryBuilder> expression;
  Collection<std::function<void()>> callbacks;
  SharedPointer<Map<String, String>> result;

private:
};

MariaDBOutputArchiver constexpr &operator%(MariaDBOutputArchiver &ar,
                                           const SharedPointer<ObjectTag> &tag) {

  if (tag->part == TagPart::Start) {
    ar.expression = QueryBuilder::create()
                        ->insert()
                        ->into("{}", tag->name.c_str())
                        ->values_start();
  }

  if (tag->part == TagPart::End) {
    ar.expression->values_end();
    ar.query(ar.expression);
  }

  return ar;
}

MariaDBOutputArchiver constexpr &
operator%(MariaDBOutputArchiver &ar, SharedPointer<KeyValueTag<double>> tag) {
  ar.expression->value(*tag->value);
  return ar;
}

MariaDBOutputArchiver constexpr &operator%(MariaDBOutputArchiver &ar,
                                           SharedPointer<KeyValueTag<int>> tag) {
  ar.expression->value(*tag->value);
  return ar;
}

MariaDBOutputArchiver constexpr &
operator%(MariaDBOutputArchiver &ar, SharedPointer<KeyValueTag<String>> tag) {
  ar.expression->value(*tag->value);
  return ar;
}
} // namespace CXORM::MariaDB
