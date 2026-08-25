// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include <media/streamer/core/caps.hpp>

namespace media::streamer {

bool caps::is_compatible_with(const caps &other) const {
  if (is_any_ || other.is_any_) {
    return true;
  }

  for (const auto &lhs : structures_) {
    for (const auto &rhs : other.structures_) {
      if (lhs.is_compatible_with(rhs)) {
        return true;
      }
    }
  }

  return false;
}

caps caps::intersect(const caps &other) const {
  if (is_any_ && other.is_any_) {
    return caps::any();
  }
  if (is_any_) {
    return other;
  }
  if (other.is_any_) {
    return *this;
  }

  // Simplified vs. GStreamer's field-level merge: keep this side's
  // structure for each compatible pair rather than merging field values
  // into a new, more specific structure. Sufficient for v1 - no consumer
  // needs a merged structure yet.
  caps result;
  for (const auto &lhs : structures_) {
    for (const auto &rhs : other.structures_) {
      if (lhs.is_compatible_with(rhs)) {
        result.add(lhs);
      }
    }
  }

  return result;
}

} // namespace media::streamer
