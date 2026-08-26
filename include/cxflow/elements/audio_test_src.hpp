// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>
#include <vector>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/elements/pcm_format.hpp>
#include <cxflow/threading/task.hpp>

namespace cxflow::elements {

// SRS-004 §5.1: synthetic waveform generator (sine, silence). Same
// task-per-source shape as fake_src.hpp - "num-buffers"/-1 unbounded is
// fake_src's own convention, reused verbatim. Unlike the other DSP
// entries in this catalog, audio_test_src is the origin of its own
// stream, so its format is declared up front via set_format() (which also
// stamps the src pad's caps, matching caps_filter's own set_caps()
// pattern) rather than resolved from a peer - there is no peer to resolve
// from until this element is already generating.
class audio_test_src : public element {
public:
  explicit audio_test_src(std::string name);

  void set_format(pcm_format fmt) {
    format_ = fmt;
    src_pad_.set_caps(pcm_caps(fmt));
  }
  void set_wave(std::string wave) { property_set("wave", std::move(wave)); } // "sine" or "silence"
  void set_frequency(double hz) { property_set("freq", hz); }
  void set_num_buffers(std::int64_t count) { property_set("num-buffers", count); } // -1 = unbounded
  void set_samples_per_buffer(std::uint64_t count) { property_set("samples-per-buffer", count); }

  static void register_type();

protected:
  state_change_return on_change_state(state from, state to) override;

private:
  void push_loop();

  std::string wave() const { return property_get<std::string>("wave").value_or("sine"); }
  double frequency() const { return property_get<double>("freq").value_or(440.0); }
  std::int64_t num_buffers() const { return property_get<std::int64_t>("num-buffers").value_or(-1); }
  std::uint64_t samples_per_buffer() const {
    return property_get<std::uint64_t>("samples-per-buffer").value_or(1024);
  }

  pad &src_pad_;
  threading::task task_;
  pcm_format format_;
  std::int64_t pushed_ = 0;
  std::uint64_t phase_sample_ = 0; // running sample index, for sine continuity across buffers
};

inline audio_test_src::audio_test_src(std::string name)
    : element(std::move(name)), src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))),
      task_([this] { push_loop(); }) {
  format_.rate = 44100;
  format_.channels = 1;
  format_.bits_per_sample = 16;
  format_.is_signed = true;
  src_pad_.set_caps(pcm_caps(format_));
  property_set("wave", std::string("sine"));
  property_set("freq", 440.0);
  property_set("num-buffers", std::int64_t{-1});
  property_set("samples-per-buffer", std::uint64_t{1024});
}

inline void audio_test_src::register_type() {
  element_factory::register_type(
      "audio_test_src", [](std::string name) { return std::make_shared<audio_test_src>(std::move(name)); });
}

inline state_change_return audio_test_src::on_change_state(state from, state to) {
  state_change_return result = element::on_change_state(from, to);
  if (result == state_change_return::failure) {
    return result;
  }

  if (from == state::paused && to == state::playing) {
    if (!task_.is_running()) {
      pushed_ = 0;
      phase_sample_ = 0;
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

inline void audio_test_src::push_loop() {
  std::int64_t limit = num_buffers();
  if (limit >= 0 && pushed_ >= limit) {
    return; // eos already sent on a previous iteration; idle until paused/stopped
  }

  std::uint64_t frames = samples_per_buffer();
  bool silence = wave() == "silence";
  double freq = frequency();
  double rate = static_cast<double>(format_.rate);

  std::vector<std::byte> data(frames * format_.bytes_per_frame());
  for (std::uint64_t f = 0; f < frames; ++f) {
    double sample = 0.0;
    if (!silence && rate > 0.0) {
      double t = static_cast<double>(phase_sample_ + f) / rate;
      sample = std::sin(2.0 * std::numbers::pi * freq * t);
    }
    for (std::uint64_t c = 0; c < format_.channels; ++c) {
      write_pcm_sample(data.data() + (f * format_.channels + c) * format_.bytes_per_sample(), format_, sample);
    }
  }
  phase_sample_ += frames;

  buffer buf(std::move(data));
  if (format_.rate > 0) {
    buf.pts = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(static_cast<double>(phase_sample_ - frames) / rate));
    buf.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(static_cast<double>(frames) / rate));
  }
  ++pushed_;

  src_pad_.push(std::move(buf));

  if (limit >= 0 && pushed_ >= limit) {
    src_pad_.send_event(event{event_type::eos});
    task_.pause(); // not stop(): stop() joins, and this runs on the task's own thread
  }
}

} // namespace cxflow::elements
