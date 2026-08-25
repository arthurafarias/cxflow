// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/testing/test_group.hpp>
#include <cxflow/core/buffer.hpp>

#include <chrono>
#include <cstddef>
#include <vector>

namespace cxflow::testing {

struct buffer_test : public test_group {
  buffer_test() : test_group("buffer", {
    {"a default-constructed buffer is empty and has no timing set", [](test_context &ctx) {
      buffer b;
      ctx.check_equal(b.size(), std::size_t{0});
      ctx.check(b.data().empty());
      ctx.check(!b.pts.has_value());
      ctx.check(!b.dts.has_value());
      ctx.check(!b.duration.has_value());
    }},
    {"copy() deep-copies storage and carries timing/offset", [](test_context &ctx) {
      std::vector<std::byte> data{std::byte{1}, std::byte{2}, std::byte{3}};
      buffer original(data);
      original.pts = std::chrono::nanoseconds(42);
      original.dts = std::chrono::nanoseconds(7);
      original.duration = std::chrono::nanoseconds(100);
      original.offset = 5;

      buffer copy = original.copy();

      ctx.check_equal(copy.size(), original.size());
      ctx.check(copy.data().data() != original.data().data(), "copy() must not share storage with the original");
      for (std::size_t i = 0; i < copy.size(); ++i) {
        ctx.check(copy.data()[i] == original.data()[i]);
      }
      ctx.check(copy.pts == original.pts, "pts should be carried by copy()");
      ctx.check(copy.dts == original.dts, "dts should be carried by copy()");
      ctx.check(copy.duration == original.duration, "duration should be carried by copy()");
      ctx.check_equal(copy.offset, original.offset);
    }},
    {"copy() of an empty buffer stays empty", [](test_context &ctx) {
      buffer b;
      buffer c = b.copy();
      ctx.check_equal(c.size(), std::size_t{0});
      ctx.check(c.data().empty());
    }},
  }) {}
};

inline static buffer_test buffer_test_instance;

} // namespace cxflow::testing
