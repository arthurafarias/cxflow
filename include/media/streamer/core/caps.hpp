// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <vector>

#include <media/streamer/core/structure.hpp>

namespace media::streamer {

// A set of alternative structures a pad can accept/produce - "compatible if
// any structure on either side is compatible". any() is a distinct third
// state from "zero structures" (which matches nothing): it is deliberately
// not representable as an empty structure list, since identity's pads need
// to mean "accepts anything" for milestone 4 to link at all.
class caps {
public:
  caps() = default;

  static caps any() {
    caps c;
    c.is_any_ = true;
    return c;
  }

  bool is_any() const { return is_any_; }

  void add(structure s) { structures_.push_back(std::move(s)); }
  const std::vector<structure> &structures() const { return structures_; }

  bool is_compatible_with(const caps &other) const;
  caps intersect(const caps &other) const;

private:
  bool is_any_ = false;
  std::vector<structure> structures_;
};

} // namespace media::streamer
