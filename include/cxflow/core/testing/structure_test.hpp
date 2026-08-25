// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <media/streamer/testing/test_group.hpp>
#include <media/streamer/core/structure.hpp>

#include <cstdint>

namespace media::streamer::testing {

struct structure_test : public test_group {
  structure_test() : test_group("structure", {
    {"same name with no fields is compatible", [](test_context &ctx) {
      structure a("audio/x-raw");
      structure b("audio/x-raw");
      ctx.check(a.is_compatible_with(b));
    }},
    {"different names are incompatible", [](test_context &ctx) {
      structure a("audio/x-raw");
      structure b("video/x-raw");
      ctx.check(!a.is_compatible_with(b));
    }},
    {"matching field values are compatible", [](test_context &ctx) {
      structure a("audio/x-raw");
      structure b("audio/x-raw");
      a.set("rate", std::int64_t{44100});
      b.set("rate", std::int64_t{44100});
      ctx.check(a.is_compatible_with(b));
    }},
    {"conflicting field values are incompatible", [](test_context &ctx) {
      structure a("audio/x-raw");
      structure b("audio/x-raw");
      a.set("rate", std::int64_t{44100});
      b.set("rate", std::int64_t{48000});
      ctx.check(!a.is_compatible_with(b));
    }},
    {"a field present on only one side does not block the match", [](test_context &ctx) {
      structure a("audio/x-raw");
      structure b("audio/x-raw");
      a.set("rate", std::int64_t{44100});
      b.set("channels", std::int64_t{2});
      ctx.check(a.is_compatible_with(b));
      ctx.check(b.is_compatible_with(a));
    }},
  }) {}
};

inline static structure_test structure_test_instance;

} // namespace media::streamer::testing
