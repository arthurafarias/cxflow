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
//
// SRS-001 §5.6.2: "num-buffers"/"interval-ms" are stored as properties on
// the inherited containers::object base, not as private fields duplicating
// that storage - set_num_buffers()/set_interval() are convenience wrappers
// over property_set(), kept for source compatibility, but the generic path
// (element->property_set("num-buffers", std::int64_t{10})) is the one the
// factory/application layer can rely on without downcasting to fake_src or
// #include-ing this header. "num-buffers" is int64_t, not uint64_t
// (REQ-5.1.4/OPEN-6, resolved: containers::variant gained a signed
// alternative) specifically so -1 ("unbounded") keeps its sentinel meaning
// instead of wrapping into a huge unsigned value.
class fake_src : public element {
public:
  explicit fake_src(std::string name);

  void set_num_buffers(std::int64_t count) { property_set("num-buffers", count); } // -1 = unbounded
  void set_interval(std::chrono::milliseconds interval) {
    property_set("interval-ms", static_cast<std::uint64_t>(interval.count()));
  }

  static void register_type();

protected:
  state_change_return on_change_state(state from, state to) override;

private:
  void push_loop();

  std::int64_t num_buffers() const { return property_get<std::int64_t>("num-buffers").value_or(-1); }
  std::chrono::milliseconds interval() const {
    return std::chrono::milliseconds(property_get<std::uint64_t>("interval-ms").value_or(0));
  }

  pad &src_pad_;
  threading::task task_;

  std::int64_t pushed_ = 0;
};

inline fake_src::fake_src(std::string name)
    : element(std::move(name)),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))),
      task_([this] { push_loop(); }) {
  property_set("num-buffers", std::int64_t{-1});
  property_set("interval-ms", std::uint64_t{0});
}

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
  std::chrono::milliseconds current_interval = interval();
  std::int64_t limit = num_buffers();

  if (current_interval.count() > 0) {
    std::this_thread::sleep_for(current_interval);
  }

  if (limit >= 0 && pushed_ >= limit) {
    return; // EOS already sent on a previous iteration; idle until paused/stopped
  }

  buffer buf;
  buf.pts = std::chrono::duration_cast<std::chrono::nanoseconds>(current_interval) * pushed_;
  ++pushed_;

  src_pad_.push(std::move(buf));

  if (limit >= 0 && pushed_ >= limit) {
    src_pad_.send_event(event{event_type::eos});
    task_.pause(); // not stop(): stop() joins, and this runs on the task's own thread
  }
}

} // namespace cxflow::elements
