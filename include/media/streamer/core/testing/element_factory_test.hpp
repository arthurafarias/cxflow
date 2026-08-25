// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <media/streamer/testing/test_group.hpp>
#include <media/streamer/core/element_factory.hpp>

#include <memory>
#include <string>
#include <utility>

namespace media::streamer::testing {

struct element_factory_test : public test_group {
  element_factory_test() : test_group("element_factory", {
    {"register_type() then create() returns a working instance", [](test_context &ctx) {
      // A name unique to this test avoids colliding with the real "fake_sink"/
      // "fake_src"/"identity" registrations elsewhere in this same test binary -
      // element_factory's registry is a single process-wide instance by design.
      element_factory::register_type("element_factory_test.basic",
                                      [](std::string name) { return std::make_shared<element>(std::move(name)); });

      auto instance = element_factory::create("element_factory_test.basic", "my-instance");
      ctx.require(instance != nullptr, "create() should return a non-null instance");
      ctx.check_equal(instance->name(), std::string("my-instance"));
    }},
    {"create() for an unknown type returns nullptr", [](test_context &ctx) {
      auto instance = element_factory::create("element_factory_test.does_not_exist", "x");
      ctx.check(instance == nullptr);
    }},
  }) {}
};

inline static element_factory_test element_factory_test_instance;

} // namespace media::streamer::testing
