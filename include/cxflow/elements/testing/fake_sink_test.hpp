// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/testing/test_group.hpp>
#include <cxflow/elements/fake_sink.hpp>

#include <chrono>
#include <cstdint>
#include <memory>

namespace cxflow::testing {

struct fake_sink_test : public test_group {
  fake_sink_test() : test_group("fake_sink", {
    {"register_type()+create() round trips through element_factory", [](test_context &ctx) {
      elements::fake_sink::register_type();
      auto instance = element_factory::create("fake_sink", "sink-instance");
      ctx.require(instance != nullptr, "element_factory should produce a fake_sink instance");
      ctx.check(instance->get_static_pad("sink") != nullptr, "fake_sink should expose a 'sink' pad");
    }},
    {"chain() counts every buffer received", [](test_context &ctx) {
      auto sink = std::make_shared<elements::fake_sink>("sink");
      pad *sink_pad = sink->get_static_pad("sink");
      ctx.require(sink_pad != nullptr, "fake_sink should expose a 'sink' pad");
      sink_pad->set_active(true);

      element upstream("upstream");
      pad &src_pad = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      ctx.require(src_pad.link(*sink_pad), "linking upstream to fake_sink should succeed");

      ctx.check_equal(sink->buffers_received(), std::uint64_t{0});
      src_pad.push(buffer{});
      src_pad.push(buffer{});
      ctx.check_equal(sink->buffers_received(), std::uint64_t{2});
    }},
    {"an EOS event posts an EOS bus message with this sink as source", [](test_context &ctx) {
      auto sink = std::make_shared<elements::fake_sink>("sink");
      auto b = std::make_shared<bus>();
      sink->set_bus(b);

      pad *sink_pad = sink->get_static_pad("sink");
      ctx.require(sink_pad != nullptr, "fake_sink should expose a 'sink' pad");

      element upstream("upstream");
      pad &src_pad = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      ctx.require(src_pad.link(*sink_pad), "linking upstream to fake_sink should succeed");

      ctx.check(src_pad.send_event(event{event_type::eos}), "send_event(eos) should succeed");

      auto popped = b->pop(std::chrono::milliseconds(0));
      ctx.require(popped.has_value(), "handle_event(eos) should post a message");
      ctx.check(popped->type == message_type::eos, "the posted message should be of type eos");
      ctx.check(popped->source.lock() == sink, "the posted message's source should be this sink");
    }},
  }) {}
};

inline static fake_sink_test fake_sink_test_instance;

} // namespace cxflow::testing
