// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/audio_convert.hpp>
#include <cxflow/elements/pcm_format.hpp>
#include <cxflow/testing/test_group.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace cxflow::testing {

struct audio_convert_test : public test_group {
  audio_convert_test() : test_group("audio_convert", {
    {"with no explicit output format, passes the input format through unchanged", [](test_context &ctx) {
      elements::audio_convert conv("conv");

      elements::pcm_format fmt;
      fmt.rate = 8000;
      fmt.channels = 1;
      fmt.bits_per_sample = 16;
      fmt.is_signed = true;

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.set_caps(elements::pcm_caps(fmt));
      upstream_src.link(*conv.get_static_pad("sink"));
      conv.get_static_pad("sink")->set_active(true);

      std::vector<buffer> received;
      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      downstream_sink.set_active(true);
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        received.push_back(std::move(buf));
        return flow_return::ok;
      });
      conv.get_static_pad("src")->link(downstream_sink);

      std::vector<std::byte> in_data{std::byte{0x34}, std::byte{0x12}}; // 0x1234 = 4660
      upstream_src.push(buffer(in_data));

      ctx.require_equal(received.size(), std::size_t{1});
      ctx.check(received[0].data().size() == in_data.size(), "no conversion should preserve byte size");
    }},
    {"downmixes stereo to mono by averaging channels", [](test_context &ctx) {
      elements::audio_convert conv("conv");
      elements::pcm_format out_fmt;
      out_fmt.rate = 8000;
      out_fmt.channels = 1;
      out_fmt.bits_per_sample = 16;
      out_fmt.is_signed = true;
      conv.set_output_format(out_fmt);

      elements::pcm_format in_fmt = out_fmt;
      in_fmt.channels = 2;

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.set_caps(elements::pcm_caps(in_fmt));
      upstream_src.link(*conv.get_static_pad("sink"));
      conv.get_static_pad("sink")->set_active(true);

      std::vector<buffer> received;
      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      downstream_sink.set_active(true);
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        received.push_back(std::move(buf));
        return flow_return::ok;
      });
      conv.get_static_pad("src")->link(downstream_sink);

      // One stereo frame: left = 16000, right = -16000 -> average ~= 0.
      std::vector<std::byte> in_data{std::byte{0x80}, std::byte{0x3E}, std::byte{0x80}, std::byte{0xC1}};
      upstream_src.push(buffer(in_data));

      ctx.require_equal(received.size(), std::size_t{1});
      auto out = received[0].data();
      ctx.require_equal(out.size(), std::size_t{2}); // one mono 16-bit sample
      std::int16_t v;
      std::memcpy(&v, out.data(), sizeof(v));
      ctx.check(std::abs(static_cast<int>(v)) <= 2, "averaging +16000/-16000 should be close to 0");
    }},
    {"converts 16-bit int PCM to 32-bit float PCM", [](test_context &ctx) {
      elements::audio_convert conv("conv");
      elements::pcm_format out_fmt;
      out_fmt.rate = 8000;
      out_fmt.channels = 1;
      out_fmt.bits_per_sample = 32;
      out_fmt.is_float = true;
      conv.set_output_format(out_fmt);

      elements::pcm_format in_fmt = out_fmt;
      in_fmt.bits_per_sample = 16;
      in_fmt.is_float = false;
      in_fmt.is_signed = true;

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.set_caps(elements::pcm_caps(in_fmt));
      upstream_src.link(*conv.get_static_pad("sink"));
      conv.get_static_pad("sink")->set_active(true);

      std::vector<buffer> received;
      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      downstream_sink.set_active(true);
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        received.push_back(std::move(buf));
        return flow_return::ok;
      });
      conv.get_static_pad("src")->link(downstream_sink);

      // Full-scale positive 16-bit sample (0x7FFF = 32767).
      std::vector<std::byte> in_data{std::byte{0xFF}, std::byte{0x7F}};
      upstream_src.push(buffer(in_data));

      ctx.require_equal(received.size(), std::size_t{1});
      auto out = received[0].data();
      ctx.require_equal(out.size(), std::size_t{4});
      float v;
      std::memcpy(&v, out.data(), sizeof(v));
      ctx.check(std::abs(static_cast<double>(v) - 1.0) < 0.001, "full-scale int16 should convert to ~1.0f");
    }},
  }) {}
};

inline static audio_convert_test audio_convert_test_instance;

} // namespace cxflow::testing
