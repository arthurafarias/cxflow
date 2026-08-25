// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace media::streamer {

// A unit of media data flowing between pads. Storage is a single contiguous
// byte block shared via shared_ptr - no pooling/reuse yet (deferred to a
// later module, along with a pluggable-allocator memory model). Copies made
// via copy() are always explicit deep copies, never implicit
// copy-on-write, to keep this first pass simple.
class buffer {
public:
  using clock_time = std::optional<std::chrono::nanoseconds>; // nullopt = "unset"

  buffer() = default;
  explicit buffer(std::vector<std::byte> data)
      : storage_(std::make_shared<std::vector<std::byte>>(std::move(data))) {}

  buffer copy() const {
    buffer result;
    if (storage_) {
      result.storage_ = std::make_shared<std::vector<std::byte>>(*storage_);
    }
    result.pts = pts;
    result.dts = dts;
    result.duration = duration;
    result.offset = offset;
    return result;
  }

  std::span<const std::byte> data() const {
    return storage_ ? std::span<const std::byte>(*storage_) : std::span<const std::byte>{};
  }

  std::size_t size() const { return storage_ ? storage_->size() : 0; }

  clock_time pts;
  clock_time dts;
  clock_time duration;
  std::uint64_t offset = 0;

private:
  std::shared_ptr<std::vector<std::byte>> storage_;
};

} // namespace media::streamer
