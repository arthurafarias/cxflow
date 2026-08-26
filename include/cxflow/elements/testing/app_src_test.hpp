// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/app_src.hpp>
#include <cxflow/testing/test_group.hpp>

namespace cxflow::testing {

struct app_src_test : public test_group {
  app_src_test() : test_group("app_src", {
    {"push_buffer() forwards to whatever is linked downstream", [](test_context &ctx) {
      elements::app_src src("src");
      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      ctx.require(src.get_static_pad("src")->link(downstream_sink), "linking app_src should succeed");
      downstream_sink.set_active(true);

      int received = 0;
      downstream_sink.set_chain_function([&](pad &, buffer) {
        ++received;
        return flow_return::ok;
      });

      ctx.check(src.push_buffer(buffer()) == flow_return::ok, "push_buffer() should forward downstream");
      ctx.check_equal(received, 1);
    }},
    {"end_of_stream() sends an eos event downstream", [](test_context &ctx) {
      elements::app_src src("src");
      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      src.get_static_pad("src")->link(downstream_sink);
      downstream_sink.set_active(true);

      bool eos_seen = false;
      downstream_sink.set_event_function([&](pad &, const event &ev) {
        eos_seen = ev.type == event_type::eos;
        return true;
      });

      src.end_of_stream();
      ctx.check(eos_seen, "end_of_stream() should send an eos event");
    }},
  }) {}
};

inline static app_src_test app_src_test_instance;

} // namespace cxflow::testing
