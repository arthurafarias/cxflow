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

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/threading/task.hpp>

namespace cxflow::elements {

// A trivial source: one src pad plus a dedicated task that generates and
// pushes buffers on its own thread while playing. Registered with
// element_factory under the type name "fake_src" (see register_types() in
// fake_src.cpp).
//
// Only ever pushes an EOS *event* downstream when done - never posts an EOS
// *message* itself. That is fake_sink's job, on receiving the event; real
// GStreamer/gstbasesink semantics.
class fake_src : public element {
public:
  explicit fake_src(std::string name);

  void set_num_buffers(std::int64_t count) { num_buffers_ = count; } // -1 = unbounded
  void set_interval(std::chrono::milliseconds interval) { interval_ = interval; }

  static void register_type();

protected:
  state_change_return on_change_state(state from, state to) override;

private:
  void push_loop();

  pad &src_pad_;
  threading::task task_;

  std::int64_t num_buffers_ = -1;
  std::chrono::milliseconds interval_{0};
  std::int64_t pushed_ = 0;
};

inline fake_src::fake_src(std::string name)
    : element(std::move(name)),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))),
      task_([this] { push_loop(); }) {}

inline void fake_src::register_type() {
  element_factory::register_type("fake_src",
                                  [](std::string name) { return std::make_shared<fake_src>(std::move(name)); });
}

inline state_change_return fake_src::on_change_state(state from, state to) {
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
    // Must be fully stopped (not merely paused) before the pipeline is torn
    // all the way down. Safe to join here: by this point playing->paused
    // already ran, so the task thread is parked in its own pause wait, and
    // this call always comes from the application thread driving
    // set_state(), never from the task thread itself.
    task_.stop();
  }

  return state_change_return::success;
}

inline void fake_src::push_loop() {
  if (interval_.count() > 0) {
    std::this_thread::sleep_for(interval_);
  }

  if (num_buffers_ >= 0 && pushed_ >= num_buffers_) {
    return; // EOS already sent on a previous iteration; idle until paused/stopped
  }

  buffer buf;
  buf.pts = std::chrono::duration_cast<std::chrono::nanoseconds>(interval_) * pushed_;
  ++pushed_;

  src_pad_.push(std::move(buf));

  if (num_buffers_ >= 0 && pushed_ >= num_buffers_) {
    src_pad_.send_event(event{event_type::eos});
    task_.pause(); // not stop(): stop() joins, and this runs on the task's own thread
  }
}

} // namespace cxflow::elements
