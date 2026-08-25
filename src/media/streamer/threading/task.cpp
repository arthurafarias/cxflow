// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include <media/streamer/threading/task.hpp>

namespace media::streamer::threading {

task::task(loop_function loop) : loop_(std::move(loop)) {}

task::~task() { stop(); }

void task::start() {
  std::unique_lock lock(mutex_);
  if (running_) {
    return;
  }
  running_ = true;
  paused_ = false;
  lock.unlock();

  thread_ = std::jthread([this](std::stop_token stop_token) { run(stop_token); });
}

void task::pause() {
  {
    std::unique_lock lock(mutex_);
    paused_ = true;
  }
}

void task::resume() {
  {
    std::unique_lock lock(mutex_);
    paused_ = false;
  }
  cond_.notify_all();
}

void task::stop() {
  {
    std::unique_lock lock(mutex_);
    if (!running_) {
      return;
    }
    running_ = false;
    paused_ = false;
  }
  cond_.notify_all();

  if (thread_.joinable()) {
    thread_.request_stop();
    thread_.join();
  }
}

bool task::is_running() const {
  std::unique_lock lock(mutex_);
  return running_;
}

void task::run(std::stop_token stop_token) {
  while (!stop_token.stop_requested()) {
    {
      std::unique_lock lock(mutex_);
      cond_.wait(lock, stop_token, [this] { return !paused_; });
    }

    if (stop_token.stop_requested()) {
      break;
    }

    if (loop_) {
      loop_();
    }
  }
}

} // namespace media::streamer::threading
