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

#include <media/streamer/core/message.hpp>
#include <media/streamer/threading/signal.hpp>

namespace media::streamer {

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

} // namespace media::streamer
