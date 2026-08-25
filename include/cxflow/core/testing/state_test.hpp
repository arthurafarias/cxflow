// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/testing/test_group.hpp>
#include <cxflow/core/state.hpp>

namespace cxflow::testing {

struct state_test : public test_group {
  state_test() : test_group("state", {
    {"state enumerators are ordered null < ready < paused < playing", [](test_context &ctx) {
      // element::set_state()'s single-step walk (see element.hpp) relies on
      // this exact ordering via static_cast<int> comparisons - reordering the
      // enum would silently break it.
      ctx.check(static_cast<int>(state::null) < static_cast<int>(state::ready), "null should order before ready");
      ctx.check(static_cast<int>(state::ready) < static_cast<int>(state::paused), "ready should order before paused");
      ctx.check(static_cast<int>(state::paused) < static_cast<int>(state::playing), "paused should order before playing");
    }},
  }) {}
};

inline static state_test state_test_instance;

} // namespace cxflow::testing
