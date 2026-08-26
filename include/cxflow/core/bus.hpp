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
#include <deque>
#include <mutex>
#include <optional>

#include <cxflow/containers/object.hpp>
#include <cxflow/core/message.hpp>
#include <cxflow/logging/journal.hpp>
#include <cxflow/threading/signal.hpp>

namespace cxflow {

// Thread-safe message queue. post() is callable from any streaming thread
// (task loops, chain/event functions); pop() is typically called from the
// thread running the application's main/bus loop.
//
// SRS-001 §7.3/§9: inherits containers::object (bus is not an element, so
// unlike bin it needs its own explicit inheritance rather than getting one
// transitively) purely for the same "walkable as a variant tree" shape
// every other observable control-plane type gets, ahead of a future
// generic serialization walk. No storage/type change to message
// delivery itself: post()/pop()/message_posted are unchanged (REQ-5.7.1),
// and queue_ stays a plain std::deque<message>, not routed through the
// inherited property map - messages are not scalar properties.
class bus : public containers::object {
public:
  void post(message msg);

  // nullopt timeout blocks indefinitely; a zero duration polls without
  // blocking. Returns nullopt on timeout with nothing posted.
  std::optional<message> pop(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

  threading::signal<bus &, const message &> message_posted;

private:
  std::mutex mutex_;
  std::condition_variable cond_;
  std::deque<message> queue_;
};

inline void bus::post(message msg) {
  // Level follows message_type's own severity - a posted error/warning is
  // exactly the kind of event journal exists to surface, while routine
  // state_changed/buffering messages stay at debug.
  switch (msg.type) {
  case message_type::error:
    journal::error("bus posting error message: {}", msg.debug_info);
    break;
  case message_type::warning:
    journal::warn("bus posting warning message: {}", msg.debug_info);
    break;
  case message_type::eos:
    journal::info("bus posting eos message: {}", msg.debug_info);
    break;
  default:
    journal::debug("bus posting {} message: {}", to_string(msg.type), msg.debug_info);
    break;
  }

  {
    std::unique_lock lock(mutex_);
    queue_.push_back(msg);
  }
  cond_.notify_one();
  message_posted(*this, msg);
}

inline std::optional<message> bus::pop(std::optional<std::chrono::milliseconds> timeout) {
  std::unique_lock lock(mutex_);

  if (timeout.has_value()) {
    if (!cond_.wait_for(lock, *timeout, [this] { return !queue_.empty(); })) {
      return std::nullopt;
    }
  } else {
    cond_.wait(lock, [this] { return !queue_.empty(); });
  }

  message msg = std::move(queue_.front());
  queue_.pop_front();
  return msg;
}

} // namespace cxflow
