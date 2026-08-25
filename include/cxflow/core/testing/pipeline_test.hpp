// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/testing/test_group.hpp>
#include <cxflow/core/pipeline.hpp>

#include <chrono>
#include <memory>

namespace cxflow::testing {

struct pipeline_test : public test_group {
  pipeline_test() : test_group("pipeline", {
    {"constructs with a non-null bus", [](test_context &ctx) {
      pipeline p("p");
      ctx.check(p.element::bus() != nullptr, "a pipeline should construct with a non-null bus");
    }},
    {"bus() hides element::bus(), returning the same underlying bus", [](test_context &ctx) {
      pipeline p("p");
      ctx.check(&p.bus() == p.element::bus().get(), "bus() should return the same underlying bus as element::bus()");
    }},
    {"a child added to the pipeline can post through its bus end to end", [](test_context &ctx) {
      pipeline p("pipe");
      auto child = std::make_shared<element>("child");
      p.add(child);

      message msg;
      msg.type = message_type::eos;
      child->post_message(msg);

      auto popped = p.bus().pop(std::chrono::milliseconds(0));
      ctx.require(popped.has_value(), "posting through the child should reach the pipeline's bus");
      ctx.check(popped->type == message_type::eos, "the popped message should be of type eos");
    }},
  }) {}
};

inline static pipeline_test pipeline_test_instance;

} // namespace cxflow::testing
