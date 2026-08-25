// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/testing/test_group.hpp>
#include <cxflow/core/bin.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace cxflow::testing {

namespace bin_test_detail {

// A minimal element whose on_change_state always fails, for exercising
// bin::on_change_state's "stop propagation on first failure" behavior.
struct failing_element : public element {
  using element::element;

protected:
  state_change_return on_change_state(state, state) override { return state_change_return::failure; }
};

} // namespace bin_test_detail

struct bin_test : public test_group {
  bin_test() : test_group("bin", {
    {"add() stores the child and propagates the bin's bus to it", [](test_context &ctx) {
      bin b("b");
      auto shared_bus = std::make_shared<bus>();
      b.set_bus(shared_bus);

      auto child = std::make_shared<element>("child");
      b.add(child);

      ctx.check_equal(b.children().size(), std::size_t{1});
      ctx.check(b.children()[0] == child, "the stored child should be the one added");
      ctx.check(child->bus() == shared_bus, "add() should propagate the bin's bus to the child");
    }},
    {"remove() drops a child", [](test_context &ctx) {
      bin b("b");
      auto child = std::make_shared<element>("child");
      b.add(child);
      b.remove(child);
      ctx.check(b.children().empty(), "remove() should drop the child from the bin");
    }},
    {"set_state() propagates sink-first upward and source-first downward", [](test_context &ctx) {
      bin b("b");
      auto src = std::make_shared<element>("src");
      auto mid = std::make_shared<element>("mid");
      auto sink = std::make_shared<element>("sink");

      pad &src_out = src->add_pad(std::make_unique<pad>("out", pad::direction::src, *src));
      pad &mid_in = mid->add_pad(std::make_unique<pad>("in", pad::direction::sink, *mid));
      pad &mid_out = mid->add_pad(std::make_unique<pad>("out", pad::direction::src, *mid));
      pad &sink_in = sink->add_pad(std::make_unique<pad>("in", pad::direction::sink, *sink));
      ctx.require(src_out.link(mid_in), "src -> mid link should succeed");
      ctx.require(mid_out.link(sink_in), "mid -> sink link should succeed");

      b.add(src);
      b.add(mid);
      b.add(sink);

      std::vector<std::string> order;
      auto record = [&](element &e, state, state) { order.push_back(e.name()); };
      src->state_changed.connect(record);
      mid->state_changed.connect(record);
      sink->state_changed.connect(record);

      ctx.require(b.set_state(state::ready) == state_change_return::success, "reaching ready should succeed");
      order.clear();

      ctx.require(b.set_state(state::paused) == state_change_return::success, "reaching paused should succeed");
      auto sink_pos = std::find(order.begin(), order.end(), std::string("sink")) - order.begin();
      auto src_pos = std::find(order.begin(), order.end(), std::string("src")) - order.begin();
      ctx.check(sink_pos < src_pos, "an upward transition should reach the sink before the source");

      order.clear();
      ctx.require(b.set_state(state::ready) == state_change_return::success, "returning to ready should succeed");
      sink_pos = std::find(order.begin(), order.end(), std::string("sink")) - order.begin();
      src_pos = std::find(order.begin(), order.end(), std::string("src")) - order.begin();
      ctx.check(src_pos < sink_pos, "a downward transition should reach the source before the sink");
    }},
    {"a child failing on_change_state stops propagation", [](test_context &ctx) {
      bin b("b");
      auto ok_child = std::make_shared<element>("ok");
      auto failing_child = std::make_shared<bin_test_detail::failing_element>("failing");
      b.add(ok_child);
      b.add(failing_child);

      ctx.check(b.set_state(state::ready) == state_change_return::failure, "a failing child's on_change_state should fail the bin's set_state()");
    }},
  }) {}
};

inline static bin_test bin_test_instance;

} // namespace cxflow::testing
