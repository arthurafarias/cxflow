// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/audio_test_src.hpp>
#include <cxflow/elements/pcm_format.hpp>
#include <cxflow/testing/test_group.hpp>

#include <chrono>
#include <cstddef>
#include <thread>
#include <vector>

namespace cxflow::testing {

struct audio_test_src_test : public test_group {
  audio_test_src_test() : test_group("audio_test_src", {
    {"generates the declared number of buffers with correct caps, then eos", [](test_context &ctx) {
      elements::audio_test_src src("src");
      elements::pcm_format fmt;
      fmt.rate = 8000;
      fmt.channels = 1;
      fmt.bits_per_sample = 16;
      fmt.is_signed = true;
      src.set_format(fmt);
      src.set_num_buffers(3);
      src.set_samples_per_buffer(10);

      auto declared = elements::pcm_format_from_caps(src.get_static_pad("src")->current_caps());
      ctx.require(declared.has_value(), "the src pad should declare audio/x-raw caps up front");
      ctx.check_equal(declared->rate, std::uint64_t{8000});

      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      src.get_static_pad("src")->link(downstream_sink);
      downstream_sink.set_active(true);

      int buffers_seen = 0;
      bool eos_seen = false;
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        ++buffers_seen;
        ctx.check_equal(buf.data().size(), std::size_t{20}); // 10 frames * 2 bytes/sample * 1 channel
        return flow_return::ok;
      });
      downstream_sink.set_event_function([&](pad &, const event &ev) {
        eos_seen = ev.type == event_type::eos;
        return true;
      });

      ctx.require(src.set_state(state::playing) == state_change_return::success, "reaching playing should succeed");
      for (int i = 0; i < 200 && !eos_seen; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      ctx.require(eos_seen, "audio_test_src should send eos once num-buffers is reached");
      ctx.check_equal(buffers_seen, 3);

      ctx.check(src.set_state(state::null) == state_change_return::success, "returning to null should succeed");
    }},
    {"wave=silence produces all-zero samples", [](test_context &ctx) {
      elements::audio_test_src src("src");
      elements::pcm_format fmt;
      fmt.rate = 8000;
      fmt.channels = 1;
      fmt.bits_per_sample = 16;
      fmt.is_signed = true;
      src.set_format(fmt);
      src.set_wave("silence");
      src.set_num_buffers(1);
      src.set_samples_per_buffer(4);

      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      src.get_static_pad("src")->link(downstream_sink);
      downstream_sink.set_active(true);

      std::vector<std::byte> received;
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        auto data = buf.data();
        received.assign(data.begin(), data.end());
        return flow_return::ok;
      });

      ctx.require(src.set_state(state::playing) == state_change_return::success, "reaching playing should succeed");
      for (int i = 0; i < 200 && received.empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      ctx.require_equal(received.size(), std::size_t{8});
      for (auto b : received) {
        ctx.check(b == std::byte{0}, "silence should produce all-zero sample bytes");
      }

      ctx.check(src.set_state(state::null) == state_change_return::success, "returning to null should succeed");
    }},
  }) {}
};

inline static audio_test_src_test audio_test_src_test_instance;

} // namespace cxflow::testing
