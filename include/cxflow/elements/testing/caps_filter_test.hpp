// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/caps_filter.hpp>
#include <cxflow/testing/test_group.hpp>

namespace cxflow::testing {

struct caps_filter_test : public test_group {
  caps_filter_test() : test_group("caps_filter", {
    {"defaults to any() caps on both pads, so any neighbor can link", [](test_context &ctx) {
      elements::caps_filter f("f");
      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      ctx.check(upstream_src.link(*f.get_static_pad("sink")), "an any() sink pad should accept any upstream");
    }},
    {"set_caps() restricts linking to a compatible neighbor", [](test_context &ctx) {
      elements::caps_filter f("f");
      structure audio("audio/x-raw");
      caps audio_caps;
      audio_caps.add(audio);
      f.set_caps(audio_caps);

      element compatible_upstream("compatible");
      pad &compatible_src = compatible_upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, compatible_upstream));
      compatible_src.set_caps(audio_caps);
      ctx.check(compatible_src.link(*f.get_static_pad("sink")), "matching structure name should link");
    }},
    {"set_caps() rejects an incompatible neighbor", [](test_context &ctx) {
      elements::caps_filter f("f");
      structure video("video/x-raw");
      caps video_caps;
      video_caps.add(video);
      f.set_caps(video_caps);

      structure audio("audio/x-raw");
      caps audio_caps;
      audio_caps.add(audio);

      element incompatible_upstream("incompatible");
      pad &incompatible_src =
          incompatible_upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, incompatible_upstream));
      incompatible_src.set_caps(audio_caps);
      ctx.check(!incompatible_src.link(*f.get_static_pad("sink")), "a mismatched structure name should fail to link");
    }},
    {"passes buffers and events through unchanged once linked", [](test_context &ctx) {
      elements::caps_filter f("f");
      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      f.get_static_pad("src")->link(downstream_sink);
      downstream_sink.set_active(true);

      bool eos_seen = false;
      downstream_sink.set_event_function([&](pad &, const event &ev) {
        eos_seen = ev.type == event_type::eos;
        return true;
      });
      int received = 0;
      downstream_sink.set_chain_function([&](pad &, buffer) {
        ++received;
        return flow_return::ok;
      });

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*f.get_static_pad("sink"));
      f.get_static_pad("sink")->set_active(true);

      upstream_src.push(buffer());
      upstream_src.send_event(event{event_type::eos});

      ctx.check_equal(received, 1);
      ctx.check(eos_seen, "eos should pass through");
    }},
  }) {}
};

inline static caps_filter_test caps_filter_test_instance;

} // namespace cxflow::testing
