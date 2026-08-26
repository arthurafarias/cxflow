// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/pcm_format.hpp>
#include <cxflow/elements/volume.hpp>
#include <cxflow/testing/test_group.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace cxflow::testing {

struct volume_test : public test_group {
  volume_test() : test_group("volume", {
    {"applies a linear gain to 16-bit signed PCM samples", [](test_context &ctx) {
      elements::volume vol("vol");
      vol.set_level(0.5);

      elements::pcm_format fmt;
      fmt.rate = 8000;
      fmt.channels = 1;
      fmt.bits_per_sample = 16;
      fmt.is_signed = true;

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.set_caps(elements::pcm_caps(fmt));
      upstream_src.link(*vol.get_static_pad("sink"));
      vol.get_static_pad("sink")->set_active(true);

      std::vector<buffer> received;
      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      downstream_sink.set_active(true);
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        received.push_back(std::move(buf));
        return flow_return::ok;
      });
      vol.get_static_pad("src")->link(downstream_sink);

      // 16000 (half of int16 max) and -16000, little-endian.
      std::vector<std::byte> in_data{std::byte{0x80}, std::byte{0x3E}, std::byte{0x80}, std::byte{0xC1}};
      upstream_src.push(buffer(in_data));

      ctx.require_equal(received.size(), std::size_t{1});
      auto out = received[0].data();
      ctx.require_equal(out.size(), in_data.size());

      auto sample_at = [&](std::size_t i) {
        std::int16_t v;
        std::memcpy(&v, out.data() + i, sizeof(v));
        return v;
      };
      // Expect roughly half the original magnitude (0.5 gain), within
      // rounding tolerance of the fixed-point conversion.
      ctx.check(std::abs(sample_at(0) - 8000) <= 2, "the first sample should be attenuated to about half");
      ctx.check(std::abs(sample_at(2) - (-8000)) <= 2, "the second sample should be attenuated to about half");
    }},
    {"forwards events unchanged", [](test_context &ctx) {
      elements::volume vol("vol");
      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      vol.get_static_pad("src")->link(downstream_sink);
      downstream_sink.set_active(true);

      bool eos_seen = false;
      downstream_sink.set_event_function([&](pad &, const event &ev) {
        eos_seen = ev.type == event_type::eos;
        return true;
      });

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*vol.get_static_pad("sink"));

      upstream_src.send_event(event{event_type::eos});
      ctx.check(eos_seen, "eos should pass through");
    }},
    {"an unresolvable format (no linked peer) reports an error instead of crashing", [](test_context &ctx) {
      elements::volume vol("vol");
      vol.get_static_pad("sink")->set_active(true);

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*vol.get_static_pad("sink")); // no caps set on upstream_src - stays any()

      ctx.check(upstream_src.push(buffer()) == flow_return::error,
                 "no resolvable audio/x-raw format should report an error, not crash");
    }},
  }) {}
};

inline static volume_test volume_test_instance;

} // namespace cxflow::testing
