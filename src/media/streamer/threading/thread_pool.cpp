// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include <media/streamer/threading/thread_pool.hpp>

#include <algorithm>
#include <iostream>

namespace media::streamer::threading {

thread_pool::thread_pool(unsigned worker_count) {
  // hardware_concurrency() may legitimately return 0 (unspecified per the
  // standard on some platforms/containers); a zero-worker pool would accept
  // submissions forever without ever running them.
  worker_count = std::max(1u, worker_count);

  workers_.reserve(worker_count);
  for (unsigned i = 0; i < worker_count; ++i) {
    workers_.emplace_back([this](std::stop_token stop_token) { worker_loop(stop_token); });
  }
}

void thread_pool::submit(task_type task) {
  {
    std::unique_lock lock(mutex_);
    queue_.push(std::move(task));
  }
  queue_cond_.notify_one();
}

thread_pool &thread_pool::instance() {
  static thread_pool pool;
  return pool;
}

void thread_pool::worker_loop(std::stop_token stop_token) {
  while (true) {
    task_type task;
    {
      std::unique_lock lock(mutex_);
      queue_cond_.wait(lock, stop_token, [this] { return !queue_.empty(); });

      if (queue_.empty()) {
        // Only reachable when stop was requested with nothing left to run.
        return;
      }

      task = std::move(queue_.front());
      queue_.pop();
    }

    try {
      task();
    } catch (const std::exception &e) {
      std::cerr << "media::streamer::threading::thread_pool: task threw: " << e.what() << '\n';
    } catch (...) {
      std::cerr << "media::streamer::threading::thread_pool: task threw a non-exception value\n";
    }
  }
}

} // namespace media::streamer::threading
