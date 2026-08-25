// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/testing/test_group.hpp>
#include <cxflow/elements/fake_src.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace cxflow::testing {

struct fake_src_test : public test_group {
  fake_src_test() : test_group("fake_src", {
    {"register_type()+create() round trips through element_factory", [](test_context &ctx) {
      elements::fake_src::register_type();
      auto instance = element_factory::create("fake_src", "src-instance");
      ctx.require(instance != nullptr, "element_factory should produce a fake_src instance");
      ctx.check(instance->get_static_pad("src") != nullptr);
    }},
    {"playing pushes buffers with pts spaced by the interval, then sends EOS at the buffer limit",
     [](test_context &ctx) {
       auto src = std::make_shared<elements::fake_src>("src");
       src->set_num_buffers(3);
       src->set_interval(std::chrono::milliseconds(5));

       pad *src_pad = src->get_static_pad("src");
       ctx.require(src_pad != nullptr, "fake_src should expose a 'src' pad");

       element downstream("downstream");
       pad &sink_pad = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
       ctx.require(src_pad->link(sink_pad), "linking fake_src to downstream should succeed");
       sink_pad.set_active(true);

       std::vector<buffer::clock_time> pts_seen;
       sink_pad.set_chain_function([&](pad &, buffer buf) {
         pts_seen.push_back(buf.pts);
         return flow_return::ok;
       });

       bool eos_seen = false;
       sink_pad.set_event_function([&](pad &, const event &ev) {
         if (ev.type == event_type::eos) {
           eos_seen = true;
         }
         return true;
       });

       ctx.require(src->set_state(state::playing) == state_change_return::success, "reaching playing should succeed");

       // The push loop runs on its own thread; poll rather than assume exact timing.
       for (int i = 0; i < 200 && !eos_seen; ++i) {
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
       }

       ctx.require(eos_seen, "fake_src should send an EOS event once num_buffers is reached");
       ctx.check_equal(pts_seen.size(), std::size_t{3});
       for (std::size_t i = 0; i < pts_seen.size(); ++i) {
         ctx.require(pts_seen[i].has_value(), "each pushed buffer should carry a pts");
         ctx.check(*pts_seen[i] == std::chrono::milliseconds(5) * static_cast<int>(i));
       }

       ctx.check(src->set_state(state::null) == state_change_return::success);
     }},
  }) {}
};

inline static fake_src_test fake_src_test_instance;

} // namespace cxflow::testing
