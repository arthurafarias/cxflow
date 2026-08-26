// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/audio_resample.hpp>
#include <cxflow/elements/pcm_format.hpp>
#include <cxflow/testing/test_group.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

namespace cxflow::testing {

namespace audio_resample_test_detail {

inline std::vector<std::byte> make_sine(std::size_t frame_count, double freq, double rate,
                                          const elements::pcm_format &fmt) {
  std::vector<std::byte> data(frame_count * fmt.bytes_per_sample());
  for (std::size_t i = 0; i < frame_count; ++i) {
    double sample = std::sin(2.0 * std::numbers::pi * freq * static_cast<double>(i) / rate);
    elements::write_pcm_sample(data.data() + i * fmt.bytes_per_sample(), fmt, sample);
  }
  return data;
}

} // namespace audio_resample_test_detail

struct audio_resample_test : public test_group {
  audio_resample_test() : test_group("audio_resample", {
    {"upsampling 2x produces twice the frame count and reconstructs input-aligned samples exactly",
     [](test_context &ctx) {
       elements::audio_resample resample("resample");
       resample.set_output_rate(16000);

       elements::pcm_format in_fmt;
       in_fmt.rate = 8000;
       in_fmt.channels = 1;
       in_fmt.bits_per_sample = 16;
       in_fmt.is_signed = true;

       element upstream("upstream");
       pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
       upstream_src.set_caps(elements::pcm_caps(in_fmt));
       upstream_src.link(*resample.get_static_pad("sink"));
       resample.get_static_pad("sink")->set_active(true);

       std::vector<buffer> received;
       element downstream("downstream");
       pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
       downstream_sink.set_active(true);
       downstream_sink.set_chain_function([&](pad &, buffer buf) {
         received.push_back(std::move(buf));
         return flow_return::ok;
       });
       resample.get_static_pad("src")->link(downstream_sink);

       constexpr std::size_t n_in = 40;
       auto in_data = audio_resample_test_detail::make_sine(n_in, 1000.0, 8000.0, in_fmt);
       upstream_src.push(buffer(in_data));

       ctx.require_equal(received.size(), std::size_t{1});
       auto out = received[0].data();
       ctx.check_equal(out.size(), std::size_t{n_in * 2 * 2}); // 2x frames, 2 bytes/sample

       // Output index 10 lands exactly on input index 5 (ratio 8000/16000 =
       // 0.5): the Lanczos kernel is 1 at offset 0 and exactly 0 at every
       // other integer offset, so this should reconstruct the original
       // input sample, not merely approximate it.
       elements::pcm_format out_fmt = in_fmt;
       out_fmt.rate = 16000;
       double reconstructed = elements::read_pcm_sample(out.data() + 10 * 2, out_fmt);
       double original = elements::read_pcm_sample(in_data.data() + 5 * 2, in_fmt);
       ctx.check(std::abs(reconstructed - original) < 0.001,
                  "an input-aligned output sample should reconstruct the original almost exactly");
     }},
    {"with no explicit output rate, passes the input through unchanged", [](test_context &ctx) {
      elements::audio_resample resample("resample");

      elements::pcm_format fmt;
      fmt.rate = 8000;
      fmt.channels = 1;
      fmt.bits_per_sample = 16;
      fmt.is_signed = true;

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.set_caps(elements::pcm_caps(fmt));
      upstream_src.link(*resample.get_static_pad("sink"));
      resample.get_static_pad("sink")->set_active(true);

      std::vector<buffer> received;
      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      downstream_sink.set_active(true);
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        received.push_back(std::move(buf));
        return flow_return::ok;
      });
      resample.get_static_pad("src")->link(downstream_sink);

      auto in_data = audio_resample_test_detail::make_sine(20, 500.0, 8000.0, fmt);
      upstream_src.push(buffer(in_data));

      ctx.require_equal(received.size(), std::size_t{1});
      ctx.check(received[0].data().size() == in_data.size(), "no output rate configured should leave frames unchanged");
    }},
    {"declares the new rate in its src pad's caps", [](test_context &ctx) {
      elements::audio_resample resample("resample");
      resample.set_output_rate(22050);

      elements::pcm_format fmt;
      fmt.rate = 44100;
      fmt.channels = 2;
      fmt.bits_per_sample = 16;
      fmt.is_signed = true;

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.set_caps(elements::pcm_caps(fmt));
      upstream_src.link(*resample.get_static_pad("sink"));
      resample.get_static_pad("sink")->set_active(true);

      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      downstream_sink.set_active(true);
      resample.get_static_pad("src")->link(downstream_sink);

      auto in_data = audio_resample_test_detail::make_sine(10, 1000.0, 44100.0, fmt);
      upstream_src.push(buffer(in_data));

      auto declared = elements::pcm_format_from_caps(resample.get_static_pad("src")->current_caps());
      ctx.require(declared.has_value(), "the src pad should declare the resampled rate in its caps");
      ctx.check_equal(declared->rate, std::uint64_t{22050});
    }},
  }) {}
};

inline static audio_resample_test audio_resample_test_instance;

} // namespace cxflow::testing
