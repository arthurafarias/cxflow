// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace media::streamer::threading {

// General-purpose pool for short-lived, fire-and-forget work (e.g. explicit
// signal::emit_async submissions). Long-running/repeating loops (pad
// streaming threads) must use task instead - submitting a repeating loop
// here would starve the pool for everyone else, since it is a fixed-size
// worker set.
class thread_pool {
public:
  using task_type = std::function<void()>;

  explicit thread_pool(unsigned worker_count = std::thread::hardware_concurrency());

  thread_pool(const thread_pool &) = delete;
  thread_pool &operator=(const thread_pool &) = delete;
  thread_pool(thread_pool &&) = delete;
  thread_pool &operator=(thread_pool &&) = delete;

  ~thread_pool() = default;

  void submit(task_type task);

  static thread_pool &instance();

private:
  void worker_loop(std::stop_token stop_token);

  std::mutex mutex_;
  std::condition_variable_any queue_cond_;
  std::queue<task_type> queue_;
  std::vector<std::jthread> workers_;
};

} // namespace media::streamer::threading
