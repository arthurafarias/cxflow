// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <unordered_map>
#include <string>

namespace CXORM::Base
{
// Deliberately has no `Null` enumerator, unlike SQLiteDataType: SQLite
// tolerates "NULL" as a (semantically odd but syntactically legal) column
// type-affinity keyword, whereas `CREATE TABLE t (col NULL)` is a straight
// syntax error in MariaDB/MySQL - NULL is a keyword there, not a type name.
// Carrying that enumerator over for cosmetic parity with SQLiteDataType
// would just be a landmine for whoever tries to use it.
enum class MariaDBDataType { Integer, Real, Text, Blob };

// Declared in MariaDBDataType's own namespace (rather than only inside
// `namespace std`) so it is ADL-findable from QueryBuilder::field()'s
// unqualified `to_string(data_type)` call regardless of include order - see
// the comment on field() in QueryBuilder.hpp.
inline std::string to_string(const MariaDBDataType& type) {

    static const std::unordered_map<MariaDBDataType, std::string> reflection_map {
        { MariaDBDataType::Integer, "INT"},
        { MariaDBDataType::Real, "DOUBLE"},
        { MariaDBDataType::Text, "TEXT"},
        { MariaDBDataType::Blob, "BLOB"},
    };

    return reflection_map.at(type);
}
}

namespace std
{
    // Thin forwarder for callers that spell this std::to_string(MariaDBDataType)
    // explicitly, mirroring SQLiteDataType.hpp's own forwarder.
    inline std::string to_string(const CXORM::Base::MariaDBDataType& type) {
        return CXORM::Base::to_string(type);
    }
}
