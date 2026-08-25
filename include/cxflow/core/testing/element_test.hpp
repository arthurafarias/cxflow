// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/testing/test_group.hpp>
#include <cxflow/core/element.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace cxflow::testing {

struct element_test : public test_group {
  element_test() : test_group("element", {
    {"add_pad() returns a reference and stores it in pads()", [](test_context &ctx) {
      element e("e");
      pad &p = e.add_pad(std::make_unique<pad>("p", pad::direction::sink, e));
      ctx.check_equal(e.pads().size(), std::size_t{1});
      ctx.check(&p == e.pads()[0].get(), "the stored pad should be the one added");
    }},
    {"get_static_pad() finds by name, nullptr when missing", [](test_context &ctx) {
      element e("e");
      e.add_pad(std::make_unique<pad>("in", pad::direction::sink, e));
      ctx.check(e.get_static_pad("in") != nullptr, "get_static_pad() should find a pad by name");
      ctx.check(e.get_static_pad("missing") == nullptr, "get_static_pad() should return nullptr for an unknown name");
    }},
    {"pad_added fires once per add_pad()", [](test_context &ctx) {
      element e("e");
      int count = 0;
      e.pad_added.connect([&](element &, pad &) { ++count; });
      e.add_pad(std::make_unique<pad>("a", pad::direction::sink, e));
      e.add_pad(std::make_unique<pad>("b", pad::direction::src, e));
      ctx.check_equal(count, 2);
    }},
    {"set_state() walks single steps, firing state_changed for each", [](test_context &ctx) {
      element e("e");
      std::vector<std::pair<state, state>> transitions;
      e.state_changed.connect([&](element &, state from, state to) { transitions.emplace_back(from, to); });

      ctx.check(e.set_state(state::paused) == state_change_return::success, "reaching paused should succeed");
      ctx.check_equal(transitions.size(), std::size_t{2});
      ctx.check(transitions[0] == std::pair(state::null, state::ready), "the first transition should be null -> ready");
      ctx.check(transitions[1] == std::pair(state::ready, state::paused), "the second transition should be ready -> paused");
      ctx.check(e.current_state() == state::paused, "current_state() should be paused after the walk");
    }},
    {"default on_change_state activates pads at ready<->paused", [](test_context &ctx) {
      element e("e");
      pad &p = e.add_pad(std::make_unique<pad>("p", pad::direction::sink, e));
      ctx.check(!p.is_active(), "a pad should not be active before set_state()");

      e.set_state(state::paused);
      ctx.check(p.is_active(), "a pad should be active once paused");

      e.set_state(state::null);
      ctx.check(!p.is_active(), "a pad should not be active once back to null");
    }},
    {"post_message() is a no-op without a bus and forwards once one is set", [](test_context &ctx) {
      element e("e");
      message unrouted;
      unrouted.type = message_type::eos;
      e.post_message(unrouted); // should not crash even though there's no bus

      auto b = std::make_shared<bus>();
      e.set_bus(b);
      ctx.check(e.bus() == b, "set_bus() should update bus()");

      message routed;
      routed.type = message_type::eos;
      e.post_message(routed);

      auto popped = b->pop(std::chrono::milliseconds(0));
      ctx.require(popped.has_value(), "post_message() should have reached the bus");
      ctx.check(popped->type == message_type::eos, "the popped message should be of type eos");
    }},
  }) {}
};

inline static element_test element_test_instance;

} // namespace cxflow::testing
