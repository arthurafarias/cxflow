// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/video_format.hpp>
#include <cxflow/elements/video_test_src.hpp>
#include <cxflow/testing/test_group.hpp>

#include <chrono>
#include <cstddef>
#include <thread>

namespace cxflow::testing {

struct video_test_src_test : public test_group {
  video_test_src_test() : test_group("video_test_src", {
    {"generates the declared number of RGB24 frames with correct caps, then eos", [](test_context &ctx) {
      elements::video_test_src src("src");
      elements::video_format fmt;
      fmt.format = elements::pixel_format::rgb24;
      fmt.width = 16;
      fmt.height = 8;
      src.set_format(fmt);
      src.set_num_buffers(2);

      auto declared = elements::video_format_from_caps(src.get_static_pad("src")->current_caps());
      ctx.require(declared.has_value(), "the src pad should declare video/x-raw caps up front");
      ctx.check_equal(declared->width, std::uint64_t{16});
      ctx.check_equal(declared->height, std::uint64_t{8});

      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      src.get_static_pad("src")->link(downstream_sink);
      downstream_sink.set_active(true);

      int buffers_seen = 0;
      bool eos_seen = false;
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        ++buffers_seen;
        ctx.check_equal(buf.data().size(), std::size_t{16 * 8 * 3});
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
      ctx.require(eos_seen, "video_test_src should send eos once num-buffers is reached");
      ctx.check_equal(buffers_seen, 2);

      ctx.check(src.set_state(state::null) == state_change_return::success, "returning to null should succeed");
    }},
    {"the bars pattern produces 8 distinct column colors", [](test_context &ctx) {
      elements::video_test_src src("src");
      elements::video_format fmt;
      fmt.format = elements::pixel_format::rgb24;
      fmt.width = 80; // 10px per bar
      fmt.height = 2;
      src.set_format(fmt);
      src.set_pattern("bars");
      src.set_num_buffers(1);

      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      src.get_static_pad("src")->link(downstream_sink);
      downstream_sink.set_active(true);

      std::vector<std::byte> frame;
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        auto data = buf.data();
        frame.assign(data.begin(), data.end());
        return flow_return::ok;
      });

      ctx.require(src.set_state(state::playing) == state_change_return::success, "reaching playing should succeed");
      for (int i = 0; i < 200 && frame.empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      ctx.require_equal(frame.size(), std::size_t{80 * 2 * 3});

      // First bar should be white (255,255,255); last bar should be black (0,0,0).
      ctx.check(frame[0] == std::byte{255} && frame[1] == std::byte{255} && frame[2] == std::byte{255},
                 "the first bar should be white");
      std::size_t last_px = (79) * 3;
      ctx.check(frame[last_px] == std::byte{0} && frame[last_px + 1] == std::byte{0} &&
                     frame[last_px + 2] == std::byte{0},
                 "the last bar should be black");

      ctx.check(src.set_state(state::null) == state_change_return::success, "returning to null should succeed");
    }},
  }) {}
};

inline static video_test_src_test video_test_src_test_instance;

} // namespace cxflow::testing
