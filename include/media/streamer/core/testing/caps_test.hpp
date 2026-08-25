// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <media/streamer/testing/test_group.hpp>
#include <media/streamer/core/caps.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace media::streamer::testing {

struct caps_test : public test_group {
  caps_test() : test_group("caps", {
    {"any() reports is_any() and is compatible with anything", [](test_context &ctx) {
      caps a = caps::any();
      ctx.check(a.is_any());

      caps b;
      b.add(structure("audio/x-raw"));
      ctx.check(a.is_compatible_with(b));
      ctx.check(b.is_compatible_with(a));
    }},
    {"a default-constructed caps is not any()", [](test_context &ctx) {
      ctx.check(!caps().is_any());
    }},
    {"multi-structure compatibility requires at least one matching pair", [](test_context &ctx) {
      caps a;
      a.add(structure("audio/x-raw"));
      a.add(structure("video/x-raw"));

      caps matching;
      matching.add(structure("video/x-raw"));
      ctx.check(a.is_compatible_with(matching));

      caps unrelated;
      unrelated.add(structure("text/plain"));
      ctx.check(!a.is_compatible_with(unrelated));
    }},
    {"intersect(): any & any stays any", [](test_context &ctx) {
      ctx.check(caps::any().intersect(caps::any()).is_any());
    }},
    {"intersect(): any & X returns X, X & any returns X", [](test_context &ctx) {
      caps x;
      x.add(structure("audio/x-raw"));

      caps left = caps::any().intersect(x);
      ctx.check(!left.is_any());
      ctx.check_equal(left.structures().size(), std::size_t{1});

      caps right = x.intersect(caps::any());
      ctx.check(!right.is_any());
      ctx.check_equal(right.structures().size(), std::size_t{1});
    }},
    {"intersect() keeps this side's structure for each compatible pair", [](test_context &ctx) {
      structure lhs_structure("audio/x-raw");
      lhs_structure.set("rate", std::int64_t{44100});
      lhs_structure.set("channels", std::int64_t{2});

      structure rhs_structure("audio/x-raw");
      rhs_structure.set("rate", std::int64_t{44100});
      rhs_structure.set("format", std::string("s16"));

      caps a;
      a.add(lhs_structure);
      caps b;
      b.add(rhs_structure);

      caps result = a.intersect(b);
      ctx.require(result.structures().size() == 1, "one compatible structure pair should produce one result structure");
      ctx.check(result.structures()[0].get("channels") != nullptr, "result should carry lhs's own fields");
      ctx.check(result.structures()[0].get("format") == nullptr, "result should not pick up rhs's fields");
    }},
  }) {}
};

inline static caps_test caps_test_instance;

} // namespace media::streamer::testing
