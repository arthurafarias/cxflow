// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/audio_rate.hpp>
#include <cxflow/elements/pcm_format.hpp>
#include <cxflow/testing/test_group.hpp>

#include <chrono>
#include <cstddef>
#include <vector>

namespace cxflow::testing {

namespace audio_rate_test_detail {

inline buffer make_frames(std::size_t frame_count, buffer::clock_time pts) {
  std::vector<std::byte> data(frame_count * 2, std::byte{0x11}); // 16-bit mono, arbitrary non-zero content
  buffer buf(std::move(data));
  buf.pts = pts;
  return buf;
}

struct fixture {
  elements::audio_rate rate_el{"rate"};
  element upstream{"upstream"};
  element downstream{"downstream"};
  pad *upstream_src;
  std::vector<buffer> received;

  fixture() {
    elements::pcm_format fmt;
    fmt.rate = 8000;
    fmt.channels = 1;
    fmt.bits_per_sample = 16;
    fmt.is_signed = true;

    upstream_src = &upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
    upstream_src->set_caps(elements::pcm_caps(fmt));
    upstream_src->link(*rate_el.get_static_pad("sink"));
    rate_el.get_static_pad("sink")->set_active(true);

    pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
    downstream_sink.set_active(true);
    downstream_sink.set_chain_function([this](pad &, buffer buf) {
      received.push_back(std::move(buf));
      return flow_return::ok;
    });
    rate_el.get_static_pad("src")->link(downstream_sink);
  }
};

} // namespace audio_rate_test_detail

struct audio_rate_test : public test_group {
  audio_rate_test() : test_group("audio_rate", {
    {"a continuously-timestamped stream passes through with no gap/overlap correction", [](test_context &ctx) {
      audio_rate_test_detail::fixture f;
      f.upstream_src->push(audio_rate_test_detail::make_frames(8, std::chrono::nanoseconds(0)));
      f.upstream_src->push(audio_rate_test_detail::make_frames(8, std::chrono::nanoseconds(1'000'000))); // 1ms = 8 frames @ 8kHz

      ctx.require_equal(f.received.size(), std::size_t{2});
      ctx.check_equal(f.received[1].data().size(), std::size_t{16}); // 8 frames * 2 bytes, unchanged
    }},
    {"a gap in timestamps is filled with inserted silence", [](test_context &ctx) {
      audio_rate_test_detail::fixture f;
      f.upstream_src->push(audio_rate_test_detail::make_frames(8, std::chrono::nanoseconds(0)));
      // Expected next pts is 1ms; this buffer claims 1.5ms - a 0.5ms (4-frame) gap.
      f.upstream_src->push(audio_rate_test_detail::make_frames(8, std::chrono::nanoseconds(1'500'000)));

      ctx.require_equal(f.received.size(), std::size_t{2});
      ctx.check_equal(f.received[1].data().size(), std::size_t{24}); // 4 silence + 8 original frames
      auto data = f.received[1].data();
      ctx.check(data[0] == std::byte{0} && data[1] == std::byte{0}, "the inserted frames should be silence");
    }},
    {"overlapping timestamps drop the overlapping leading frames", [](test_context &ctx) {
      audio_rate_test_detail::fixture f;
      f.upstream_src->push(audio_rate_test_detail::make_frames(8, std::chrono::nanoseconds(0)));
      // Expected next pts is 1ms; this buffer claims 0.5ms - a 0.5ms (4-frame) overlap.
      f.upstream_src->push(audio_rate_test_detail::make_frames(8, std::chrono::nanoseconds(500'000)));

      ctx.require_equal(f.received.size(), std::size_t{2});
      ctx.check_equal(f.received[1].data().size(), std::size_t{8}); // 4 of the 8 frames dropped
    }},
  }) {}
};

inline static audio_rate_test audio_rate_test_instance;

} // namespace cxflow::testing
