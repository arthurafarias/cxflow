// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <CXORM/Modules/SQL/Base/SQLiteDataType.hpp>
#include <CXORM/Testing/TestGroup.hpp>

#include <stdexcept>
#include <string>

namespace CXORM::Base::Testing {

// std::to_string(SQLiteDataType) is implemented as a
// `static const std::unordered_map<SQLiteDataType, std::string> reflection_map`
// looked up via .at() - a *partial* function with no default case. Every
// enumerator declared in SQLiteDataType.hpp happens to have a map entry
// today, so exercising every branch of that map (not just the Integer/Text
// used by the examples) needs an explicit round trip per enumerator, plus a
// value that was never a declared enumerator to prove .at() really does
// throw rather than silently returning something.
inline static ::CXORM::Testing::TestGroup SQLiteDataTypeTest{
    "SQLiteDataType",
    {
        {"ToStringCoversEveryDeclaredEnumerator",
         [](auto &ctx) {
           ctx.check_equal(std::to_string(SQLiteDataType::Null), "NULL");
           ctx.check_equal(std::to_string(SQLiteDataType::Integer), "INTEGER");
           ctx.check_equal(std::to_string(SQLiteDataType::Real), "REAL");
           ctx.check_equal(std::to_string(SQLiteDataType::Text), "TEXT");
           ctx.check_equal(std::to_string(SQLiteDataType::Blob), "BLOB");
         }},

        {"ToStringOnOutOfRangeValueThrows",
         [](auto &ctx) {
           auto bogus = static_cast<SQLiteDataType>(999);
           ctx.template check_throws<std::out_of_range>(
               [&] { (void)std::to_string(bogus); }, "std::to_string(bogus)");
         }},
    }};

} // namespace CXORM::Base::Testing
