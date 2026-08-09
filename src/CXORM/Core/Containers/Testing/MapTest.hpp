// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <CXORM/Core/Containers/Map.hpp>
#include <CXORM/Core/Containers/String.hpp>
#include <CXORM/Testing/TestGroup.hpp>

#include <stdexcept>

namespace CXORM::Core::Containers::Testing {

// Row (used throughout the SQL/serialization modules) is just
// Map<String, String>, a thin wrapper over std::map. This confirms it keeps
// std::map's contract - callers rely on row.at(...) throwing rather than
// silently returning a default value for a missing column.
inline static ::CXORM::Testing::TestGroup MapTest{
    "Map",
    {
        {"OperatorBracketInsertsDefaultOnMissingKey",
         [](auto &ctx) {
           Map<String, String> row;
           row["age"] = "36";
           ctx.check_equal(row["age"], "36");
           ctx.check_equal(row["missing"], "");
         }},

        {"AtThrowsOutOfRangeOnMissingKey",
         [](auto &ctx) {
           Map<String, String> row;
           row["age"] = "36";
           ctx.template check_throws<std::out_of_range>(
               [&] { (void)row.at("missing"); }, "row.at(\"missing\")");
         }},
    }};

} // namespace CXORM::Core::Containers::Testing
