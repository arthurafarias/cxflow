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

#include <cxflow/core/message.hpp>
#include <cxflow/threading/signal.hpp>

namespace cxflow {

// Thread-safe message queue. post() is callable from any streaming thread
// (task loops, chain/event functions); pop() is typically called from the
// thread running the application's main/bus loop.
class bus {
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
