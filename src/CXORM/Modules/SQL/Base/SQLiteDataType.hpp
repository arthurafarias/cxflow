#pragma once

#include <unordered_map>
#include <string>
namespace CXORM::Base
{
enum class SQLiteDataType { Null, Integer, Real, Text, Blob };
}

namespace std
{
    inline std::string to_string(const CXORM::Base::SQLiteDataType& type) {

        static const std::unordered_map<CXORM::Base::SQLiteDataType, std::string> reflection_map {
            { CXORM::Base::SQLiteDataType::Null, "NULL"},
            { CXORM::Base::SQLiteDataType::Integer, "INTEGER"},
            { CXORM::Base::SQLiteDataType::Real, "REAL"},
            { CXORM::Base::SQLiteDataType::Text, "TEXT"},
            { CXORM::Base::SQLiteDataType::Blob, "BLOB"},
        };

        return reflection_map.at(type);
    }
}