// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/testing/test_group.hpp>
#include <cxflow/elements/identity.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace cxflow::testing {

struct identity_test : public test_group {
  identity_test() : test_group("identity", {
    {"register_type()+create() round trips through element_factory", [](test_context &ctx) {
      elements::identity::register_type();
      auto instance = element_factory::create("identity", "identity-instance");
      ctx.require(instance != nullptr, "element_factory should produce an identity instance");
      ctx.check(instance->get_static_pad("sink") != nullptr);
      ctx.check(instance->get_static_pad("src") != nullptr);
    }},
    {"chain() re-pushes buffers downstream unchanged", [](test_context &ctx) {
      auto id = std::make_shared<elements::identity>("id");
      pad *sink_pad = id->get_static_pad("sink");
      pad *src_pad = id->get_static_pad("src");
      ctx.require(sink_pad != nullptr && src_pad != nullptr, "identity should expose 'sink' and 'src' pads");
      sink_pad->set_active(true);

      element upstream("upstream"), downstream("downstream");
      pad &up_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      pad &down_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      ctx.require(up_src.link(*sink_pad), "upstream -> identity link should succeed");
      ctx.require(src_pad->link(down_sink), "identity -> downstream link should succeed");
      down_sink.set_active(true);

      std::vector<std::uint64_t> offsets_seen;
      down_sink.set_chain_function([&](pad &, buffer buf) {
        offsets_seen.push_back(buf.offset);
        return flow_return::ok;
      });

      buffer buf;
      buf.offset = 42;
      ctx.check(up_src.push(std::move(buf)) == flow_return::ok);
      ctx.require(offsets_seen.size() == 1, "the buffer should have reached downstream");
      ctx.check_equal(offsets_seen[0], std::uint64_t{42});
    }},
    {"events are re-sent downstream", [](test_context &ctx) {
      auto id = std::make_shared<elements::identity>("id");
      pad *sink_pad = id->get_static_pad("sink");
      pad *src_pad = id->get_static_pad("src");
      ctx.require(sink_pad != nullptr && src_pad != nullptr, "identity should expose 'sink' and 'src' pads");

      element upstream("upstream"), downstream("downstream");
      pad &up_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      pad &down_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      ctx.require(up_src.link(*sink_pad), "upstream -> identity link should succeed");
      ctx.require(src_pad->link(down_sink), "identity -> downstream link should succeed");

      bool eos_seen = false;
      down_sink.set_event_function([&](pad &, const event &ev) {
        if (ev.type == event_type::eos) {
          eos_seen = true;
        }
        return true;
      });

      ctx.check(up_src.send_event(event{event_type::eos}));
      ctx.check(eos_seen);
    }},
  }) {}
};

inline static identity_test identity_test_instance;

} // namespace cxflow::testing
