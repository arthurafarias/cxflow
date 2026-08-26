// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <variant>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/logging/journal.hpp>
#include <cxflow/threading/task.hpp>

namespace cxflow::elements {

// SRS-004 §5.1: thread-decoupling buffer with watermarks. One sink pad, one
// src pad; a dedicated drain task (mirroring fake_src's own task-per-source
// shape) pops items and forwards them downstream on its own thread, so the
// upstream chain() call returns immediately instead of blocking on
// downstream processing. Buffers and events share one internal queue (not
// two separate ones) so relative ordering between them - in particular
// "eos arrives after every buffer already queued" - is preserved exactly
// as it arrived.
//
// Simplification vs. GStreamer's queue: GStreamer blocks the producer once
// "max-size-buffers" is hit (the actual thread-decoupling contract). This
// codebase's chain() has no cooperative cancellation point a blocked
// producer could be released from during shutdown (unlike GStreamer's own
// GCond wired into its state-change machinery), so blocking here risks a
// producer thread stuck past pause()/stop() with no path to interrupt it.
// queue instead drops the oldest queued item once full - still bounded
// memory, still real thread decoupling, without the shutdown-deadlock
// hazard a blocking policy would introduce into this codebase's simpler
// threading model.
class queue : public element {
public:
  explicit queue(std::string name);

  // 0 = unbounded (accepted, but defeats the "watermark" point - callers
  // wanting real bounding should set a positive limit).
  void set_max_size_buffers(std::uint64_t count) { property_set("max-size-buffers", count); }

  static void register_type();

protected:
  state_change_return on_change_state(state from, state to) override;

private:
  struct item {
    bool is_buffer;
    buffer buf;
    event ev;
  };

  std::uint64_t max_size_buffers() const { return property_get<std::uint64_t>("max-size-buffers").value_or(200); }

  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);
  void drain_loop();

  pad &sink_pad_;
  pad &src_pad_;
  threading::task task_;

  std::mutex mutex_;
  std::condition_variable cond_;
  std::deque<item> items_;
};

inline queue::queue(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))),
      task_([this] { drain_loop(); }) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
  property_set("max-size-buffers", std::uint64_t{200});
}

inline void queue::register_type() {
  element_factory::register_type("queue", [](std::string name) { return std::make_shared<queue>(std::move(name)); });
}

inline state_change_return queue::on_change_state(state from, state to) {
  state_change_return result = element::on_change_state(from, to);
  if (result == state_change_return::failure) {
    return result;
  }

  if (from == state::paused && to == state::playing) {
    if (!task_.is_running()) {
      task_.start();
    } else {
      task_.resume();
    }
  } else if (from == state::playing && to == state::paused) {
    task_.pause();
  } else if (from == state::ready && to == state::null) {
    task_.stop();
    std::unique_lock lock(mutex_);
    items_.clear();
  }

  return state_change_return::success;
}

inline flow_return queue::chain(pad & /*sink_pad*/, buffer buf) {
  std::unique_lock lock(mutex_);
  auto limit = max_size_buffers();
  if (limit > 0 && items_.size() >= limit) {
    journal::warn("queue '{}' is full ({} items), dropping the oldest queued item", name(), limit);
    items_.pop_front();
  }
  items_.push_back(item{true, std::move(buf), {}});
  lock.unlock();
  cond_.notify_all();
  return flow_return::ok;
}

inline bool queue::handle_event(pad & /*sink_pad*/, const event &ev) {
  std::unique_lock lock(mutex_);
  items_.push_back(item{false, {}, ev});
  lock.unlock();
  cond_.notify_all();
  return true;
}

inline void queue::drain_loop() {
  item next{false, {}, {}};
  {
    std::unique_lock lock(mutex_);
    if (!cond_.wait_for(lock, std::chrono::milliseconds(20), [this] { return !items_.empty(); })) {
      return; // nothing ready this tick - the task's own loop calls us again
    }
    next = std::move(items_.front());
    items_.pop_front();
  }

  if (next.is_buffer) {
    src_pad_.push(std::move(next.buf));
  } else {
    src_pad_.send_event(next.ev);
  }
}

} // namespace cxflow::elements
