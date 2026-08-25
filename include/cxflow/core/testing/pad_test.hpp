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
#include <cxflow/core/pad.hpp>

#include <cstddef>
#include <vector>

namespace cxflow::testing {

struct pad_test : public test_group {
  pad_test() : test_group("pad", {
    {"link() connects a src pad to a sink pad", [](test_context &ctx) {
      element src_owner("src-owner"), sink_owner("sink-owner");
      pad src_pad("src", pad::direction::src, src_owner);
      pad sink_pad("sink", pad::direction::sink, sink_owner);

      ctx.check(src_pad.link(sink_pad), "link() should succeed for a src->sink pairing");
      ctx.check(src_pad.is_linked(), "the src pad should be linked");
      ctx.check(sink_pad.is_linked(), "the sink pad should be linked");
      ctx.check(src_pad.peer() == &sink_pad, "the src pad's peer should be the sink pad");
      ctx.check(sink_pad.peer() == &src_pad, "the sink pad's peer should be the src pad");
    }},
    {"link() fails for a non-src->sink direction pairing", [](test_context &ctx) {
      element owner_a("a"), owner_b("b");
      pad a_pad("a", pad::direction::sink, owner_a);
      pad b_pad("b", pad::direction::sink, owner_b);
      ctx.check(!a_pad.link(b_pad), "link() should fail for a sink->sink pairing");
    }},
    {"link() fails when either side is already linked", [](test_context &ctx) {
      element o1("o1"), o2("o2"), o3("o3");
      pad p1("p1", pad::direction::src, o1);
      pad p2("p2", pad::direction::sink, o2);
      pad p3("p3", pad::direction::sink, o3);

      ctx.check(p1.link(p2), "the first link() call should succeed");
      ctx.check(!p1.link(p3), "linking an already-linked pad again should fail");
    }},
    {"link() fails when neither side's caps are compatible", [](test_context &ctx) {
      element src_owner("src-owner"), sink_owner("sink-owner");
      pad src_pad("src", pad::direction::src, src_owner);
      pad sink_pad("sink", pad::direction::sink, sink_owner);

      caps video_caps;
      video_caps.add(structure("video/x-raw"));
      caps audio_caps;
      audio_caps.add(structure("audio/x-raw"));
      src_pad.set_caps(video_caps);
      sink_pad.set_caps(audio_caps);

      ctx.check(!src_pad.link(sink_pad), "link() should fail when caps are incompatible");
    }},
    {"unlink() clears the peer on both sides", [](test_context &ctx) {
      element o1("o1"), o2("o2");
      pad a("a", pad::direction::src, o1);
      pad b("b", pad::direction::sink, o2);
      a.link(b);
      a.unlink();
      ctx.check(!a.is_linked(), "unlink() should clear the src side's linked state");
      ctx.check(!b.is_linked(), "unlink() should clear the sink side's linked state");
    }},
    {"push() with no peer returns not_linked", [](test_context &ctx) {
      element o("o");
      pad p("p", pad::direction::src, o);
      ctx.check(p.push(buffer{}) == flow_return::not_linked, "push() with no peer should return not_linked");
    }},
    {"push() to an inactive linked pad returns flushing without invoking chain", [](test_context &ctx) {
      element src_owner("src-owner"), sink_owner("sink-owner");
      pad src_pad("src", pad::direction::src, src_owner);
      pad sink_pad("sink", pad::direction::sink, sink_owner);
      ctx.require(src_pad.link(sink_pad), "link() should succeed for this setup");

      bool chain_called = false;
      sink_pad.set_chain_function([&](pad &, buffer) {
        chain_called = true;
        return flow_return::ok;
      });

      ctx.check(src_pad.push(buffer{}) == flow_return::flushing, "push() to an inactive pad should return flushing");
      ctx.check(!chain_called, "the chain function should not be invoked while inactive");

      sink_pad.set_active(true);
      ctx.check(src_pad.push(buffer{}) == flow_return::ok, "push() to an active pad should return ok");
      ctx.check(chain_called, "the chain function should be invoked once active");
    }},
    {"push() to an active linked pad with no chain function returns error", [](test_context &ctx) {
      element src_owner("src-owner"), sink_owner("sink-owner");
      pad src_pad("src", pad::direction::src, src_owner);
      pad sink_pad("sink", pad::direction::sink, sink_owner);
      ctx.require(src_pad.link(sink_pad), "link() should succeed for this setup");
      sink_pad.set_active(true);

      ctx.check(src_pad.push(buffer{}) == flow_return::error, "push() with no chain function should return error");
    }},
    {"buffer_probe fires on push()", [](test_context &ctx) {
      element src_owner("src-owner"), sink_owner("sink-owner");
      pad src_pad("src", pad::direction::src, src_owner);
      pad sink_pad("sink", pad::direction::sink, sink_owner);
      ctx.require(src_pad.link(sink_pad), "link() should succeed for this setup");
      sink_pad.set_active(true);
      sink_pad.set_chain_function([](pad &, buffer) { return flow_return::ok; });

      int probe_count = 0;
      src_pad.buffer_probe.connect([&](pad &, const buffer &) { ++probe_count; });
      src_pad.push(buffer{});
      ctx.check_equal(probe_count, 1);
    }},
    {"send_event() toggles flushing and dispatches to the peer's event function", [](test_context &ctx) {
      element src_owner("src-owner"), sink_owner("sink-owner");
      pad src_pad("src", pad::direction::src, src_owner);
      pad sink_pad("sink", pad::direction::sink, sink_owner);
      ctx.require(src_pad.link(sink_pad), "link() should succeed for this setup");
      sink_pad.set_active(true);
      sink_pad.set_chain_function([](pad &, buffer) { return flow_return::ok; });

      std::vector<event_type> seen;
      sink_pad.set_event_function([&](pad &, const event &ev) {
        seen.push_back(ev.type);
        return true;
      });

      ctx.check(src_pad.send_event(event{event_type::flush_start}), "send_event(flush_start) should succeed");
      ctx.check(src_pad.push(buffer{}) == flow_return::flushing, "push() should observe the flushing state");

      ctx.check(src_pad.send_event(event{event_type::flush_stop}), "send_event(flush_stop) should succeed");
      ctx.check(src_pad.push(buffer{}) == flow_return::ok, "push() should observe flushing cleared");

      ctx.check_equal(seen.size(), std::size_t{2});
      ctx.check(seen[0] == event_type::flush_start, "the first dispatched event should be flush_start");
      ctx.check(seen[1] == event_type::flush_stop, "the second dispatched event should be flush_stop");
    }},
    {"send_event() with no peer returns false", [](test_context &ctx) {
      element o("o");
      pad p("p", pad::direction::src, o);
      ctx.check(!p.send_event(event{event_type::eos}), "send_event() with no peer should return false");
    }},
  }) {}
};

inline static pad_test pad_test_instance;

} // namespace cxflow::testing
