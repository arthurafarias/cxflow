// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include <media/streamer/core/bus.hpp>

namespace media::streamer {

void bus::post(message msg) {
  {
    std::unique_lock lock(mutex_);
    queue_.push_back(msg);
  }
  cond_.notify_one();
  message_posted(*this, msg);
}

std::optional<message> bus::pop(std::optional<std::chrono::milliseconds> timeout) {
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

} // namespace media::streamer
