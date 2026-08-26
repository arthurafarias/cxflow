// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/app_sink.hpp>
#include <cxflow/testing/test_group.hpp>

namespace cxflow::testing {

struct app_sink_test : public test_group {
  app_sink_test() : test_group("app_sink", {
    {"try_pull_buffer() returns queued buffers in order, then nullopt", [](test_context &ctx) {
      elements::app_sink sink("sink");
      pad *sink_pad = sink.get_static_pad("sink");
      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*sink_pad);
      sink_pad->set_active(true);

      buffer first;
      first.offset = 1;
      buffer second;
      second.offset = 2;
      upstream_src.push(first);
      upstream_src.push(second);

      auto pulled_first = sink.try_pull_buffer();
      auto pulled_second = sink.try_pull_buffer();
      auto pulled_third = sink.try_pull_buffer();

      ctx.require(pulled_first.has_value() && pulled_second.has_value(), "two pushed buffers should be pullable");
      ctx.check_equal(pulled_first->offset, std::uint64_t{1});
      ctx.check_equal(pulled_second->offset, std::uint64_t{2});
      ctx.check(!pulled_third.has_value(), "pulling with nothing queued should return nullopt");
    }},
    {"buffer_received/eos_received signals fire for a fully event-driven consumer", [](test_context &ctx) {
      elements::app_sink sink("sink");
      pad *sink_pad = sink.get_static_pad("sink");
      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*sink_pad);
      sink_pad->set_active(true);

      int buffers_seen = 0;
      sink.buffer_received.connect([&](elements::app_sink &, const buffer &) { ++buffers_seen; });
      bool eos_seen = false;
      sink.eos_received.connect([&](elements::app_sink &) { eos_seen = true; });

      upstream_src.push(buffer());
      upstream_src.send_event(event{event_type::eos});

      ctx.check_equal(buffers_seen, 1);
      ctx.check(eos_seen, "eos_received should fire on eos");
    }},
  }) {}
};

inline static app_sink_test app_sink_test_instance;

} // namespace cxflow::testing
