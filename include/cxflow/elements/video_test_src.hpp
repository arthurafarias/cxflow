// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/elements/video_format.hpp>
#include <cxflow/threading/task.hpp>

namespace cxflow::elements {

// SRS-004 §5.1: synthetic pattern generator (bars, checkerboard), RGB24
// only (video_convert.hpp handles I420 elsewhere in the chain). Same
// task-per-source/"num-buffers"+"interval-ms" shape as fake_src.hpp - the
// format is declared up front via set_format() (stamping the src pad's
// caps immediately), the same reasoning as audio_test_src.hpp's own
// comment: there is no peer to resolve a format from until this element is
// already generating.
//
// "bars" is a simplified 8-equal-column color-bar pattern (white, yellow,
// cyan, green, magenta, red, blue, black, the same 8 hues real SMPTE bars
// use), not full SMPTE-standard bars with their chroma/PLUGE sub-regions.
class video_test_src : public element {
public:
  explicit video_test_src(std::string name);

  void set_format(video_format fmt) {
    format_ = fmt;
    src_pad_.set_caps(video_caps(fmt));
  }
  void set_pattern(std::string pattern) { property_set("pattern", std::move(pattern)); } // "bars" or "checkerboard"
  void set_num_buffers(std::int64_t count) { property_set("num-buffers", count); }       // -1 = unbounded
  void set_interval(std::chrono::milliseconds interval) {
    property_set("interval-ms", static_cast<std::uint64_t>(interval.count()));
  }

  static void register_type();

protected:
  state_change_return on_change_state(state from, state to) override;

private:
  void push_loop();
  std::vector<std::byte> render_frame() const;

  std::string pattern() const { return property_get<std::string>("pattern").value_or("bars"); }
  std::int64_t num_buffers() const { return property_get<std::int64_t>("num-buffers").value_or(-1); }
  std::chrono::milliseconds interval() const {
    return std::chrono::milliseconds(property_get<std::uint64_t>("interval-ms").value_or(0));
  }

  pad &src_pad_;
  threading::task task_;
  video_format format_;
  std::int64_t pushed_ = 0;
};

inline video_test_src::video_test_src(std::string name)
    : element(std::move(name)), src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))),
      task_([this] { push_loop(); }) {
  format_.format = pixel_format::rgb24;
  format_.width = 320;
  format_.height = 240;
  format_.framerate_num = 25;
  format_.framerate_den = 1;
  src_pad_.set_caps(video_caps(format_));
  property_set("pattern", std::string("bars"));
  property_set("num-buffers", std::int64_t{-1});
  property_set("interval-ms", std::uint64_t{0});
}

inline void video_test_src::register_type() {
  element_factory::register_type(
      "video_test_src", [](std::string name) { return std::make_shared<video_test_src>(std::move(name)); });
}

inline state_change_return video_test_src::on_change_state(state from, state to) {
  state_change_return result = element::on_change_state(from, to);
  if (result == state_change_return::failure) {
    return result;
  }

  if (from == state::paused && to == state::playing) {
    if (!task_.is_running()) {
      pushed_ = 0;
      task_.start();
    } else {
      task_.resume();
    }
  } else if (from == state::playing && to == state::paused) {
    task_.pause();
  } else if (from == state::ready && to == state::null) {
    task_.stop();
  }

  return state_change_return::success;
}

inline std::vector<std::byte> video_test_src::render_frame() const {
  std::vector<std::byte> data(format_.frame_size());
  if (format_.format != pixel_format::rgb24 || format_.width == 0 || format_.height == 0) {
    return data;
  }

  bool bars = pattern() != "checkerboard";
  static constexpr std::uint8_t bar_colors[8][3] = {
      {255, 255, 255}, {255, 255, 0}, {0, 255, 255}, {0, 255, 0},
      {255, 0, 255},   {255, 0, 0},   {0, 0, 255},   {0, 0, 0},
  };

  for (std::uint64_t row = 0; row < format_.height; ++row) {
    for (std::uint64_t col = 0; col < format_.width; ++col) {
      std::uint8_t r, g, b;
      if (bars) {
        std::uint64_t bar = (col * 8) / format_.width;
        bar = bar > 7 ? 7 : bar;
        r = bar_colors[bar][0];
        g = bar_colors[bar][1];
        b = bar_colors[bar][2];
      } else {
        constexpr std::uint64_t block = 20;
        bool white = ((row / block) + (col / block)) % 2 == 0;
        r = g = b = white ? 255 : 0;
      }
      std::uint64_t px = (row * format_.width + col) * 3;
      data[px] = static_cast<std::byte>(r);
      data[px + 1] = static_cast<std::byte>(g);
      data[px + 2] = static_cast<std::byte>(b);
    }
  }
  return data;
}

inline void video_test_src::push_loop() {
  std::int64_t limit = num_buffers();
  if (limit >= 0 && pushed_ >= limit) {
    return; // eos already sent on a previous iteration; idle until paused/stopped
  }

  auto current_interval = interval();
  if (current_interval.count() > 0) {
    std::this_thread::sleep_for(current_interval);
  }

  buffer buf(render_frame());
  if (format_.framerate_num > 0) {
    double frame_seconds = static_cast<double>(format_.framerate_den) / static_cast<double>(format_.framerate_num);
    buf.pts = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(static_cast<double>(pushed_) * frame_seconds));
    buf.duration =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(frame_seconds));
  }
  ++pushed_;

  src_pad_.push(std::move(buf));

  if (limit >= 0 && pushed_ >= limit) {
    src_pad_.send_event(event{event_type::eos});
    task_.pause(); // not stop(): stop() joins, and this runs on the task's own thread
  }
}

} // namespace cxflow::elements
