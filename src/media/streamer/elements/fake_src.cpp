// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include <media/streamer/elements/fake_src.hpp>

#include <thread>

#include <media/streamer/core/element_factory.hpp>
#include <media/streamer/core/event.hpp>

namespace media::streamer::elements {

fake_src::fake_src(std::string name)
    : element(std::move(name)),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))),
      task_([this] { push_loop(); }) {}

void fake_src::register_type() {
  element_factory::register_type("fake_src",
                                  [](std::string name) { return std::make_shared<fake_src>(std::move(name)); });
}

state_change_return fake_src::on_change_state(state from, state to) {
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

void fake_src::push_loop() {
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

} // namespace media::streamer::elements
