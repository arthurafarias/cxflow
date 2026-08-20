#pragma once

#include <unordered_map>
#include <string>

namespace CXORM::Base
{
enum class SQLiteDataType { Null, Integer, Real, Text, Blob };

// Declared in SQLiteDataType's own namespace (rather than only inside
// `namespace std`) so it is ADL-findable from QueryBuilder::field()'s
// unqualified `to_string(data_type)` call regardless of include order - see
// the comment on field() in QueryBuilder.hpp.
inline std::string to_string(const SQLiteDataType& type) {

    static const std::unordered_map<SQLiteDataType, std::string> reflection_map {
        { SQLiteDataType::Null, "NULL"},
        { SQLiteDataType::Integer, "INTEGER"},
        { SQLiteDataType::Real, "REAL"},
        { SQLiteDataType::Text, "TEXT"},
        { SQLiteDataType::Blob, "BLOB"},
    };

    return reflection_map.at(type);
}
}

namespace std
{
    // Thin forwarder kept for existing call sites that spell this
    // std::to_string(SQLiteDataType) explicitly.
    inline std::string to_string(const CXORM::Base::SQLiteDataType& type) {
        return CXORM::Base::to_string(type);
    }
}