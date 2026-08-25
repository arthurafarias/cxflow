// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <memory>
#include <utility>

#include <cxflow/core/bin.hpp>
#include <cxflow/core/bus.hpp>

namespace cxflow {

// A bin that owns a bus for asynchronous message delivery to the
// application driving it. bin::add() propagates this bus down to every
// child added, which is how fake_sink (and any future element) reaches a
// bus to post to without holding a parent pointer. Full clock/base-time
// synchronized playback is deferred - pipeline holds no clock in this pass.
class pipeline : public bin {
public:
  explicit pipeline(std::string name) : bin(std::move(name)) { set_bus(std::make_shared<class bus>()); }

  // Always non-null: the constructor above guarantees it. Hides (not
  // overrides - non-virtual) element::bus(), which returns a nullable
  // shared_ptr for elements that may not belong to a pipeline yet.
  class bus &bus() { return *element::bus(); }
};

} // namespace cxflow
