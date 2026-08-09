// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <CXORM/Core/Containers/Collection.hpp>
#include <CXORM/Testing/TestGroup.hpp>

#include <functional>

namespace CXORM::Core::Containers::Testing {

inline static ::CXORM::Testing::TestGroup CollectionTest{
    "Collection",
    {
        {"BehavesAsADequeForBasicPushAndIndexing",
         [](auto &ctx) {
           Collection<int> items;
           items.push_back(1);
           items.push_back(2);
           items.push_back(3);
           if (!ctx.require_equal(items.size(), 3u, "items.size()")) {
             return;
           }
           ctx.check_equal(items[0], 1);
           ctx.check_equal(items[2], 3);
         }},

        {"TransformProducesANewCollectionOfMappedValues",
         [](auto &ctx) {
           Collection<int> items{1, 2, 3};
           auto doubled = items.transform<int>(
               std::function<int(const int &)>([](const int &v) { return v * 2; }));
           if (!ctx.require_equal(doubled.size(), 3u, "doubled.size()")) {
             return;
           }
           ctx.check_equal(doubled[0], 2);
           ctx.check_equal(doubled[1], 4);
           ctx.check_equal(doubled[2], 6);
         }},

        {"TransformOnEmptyCollectionIsEmpty",
         [](auto &ctx) {
           Collection<int> items;
           auto doubled = items.transform<int>(
               std::function<int(const int &)>([](const int &v) { return v * 2; }));
           ctx.check_equal(doubled.size(), 0u);
         }},
    }};

} // namespace CXORM::Core::Containers::Testing
