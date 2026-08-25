// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <vector>

#include <cxflow/containers/object.hpp>
#include <cxflow/core/structure.hpp>

namespace cxflow {

// A set of alternative structures a pad can accept/produce - "compatible if
// any structure on either side is compatible". any() is a distinct third
// state from "zero structures" (which matches nothing): it is deliberately
// not representable as an empty structure list, since identity's pads need
// to mean "accepts anything" for milestone 4 to link at all.
//
// SRS-001 §5.4/§7.3: inherits containers::object (unused by is_any_/
// structures_ themselves, which stay plain members - is_compatible_with/
// intersect are structural value comparisons, not live subscriptions,
// REQ-5.4.2) purely so caps gains the same "walkable as a variant tree"
// shape every other observable control-plane type in this SRS has, for a
// future generic serialization walk (§7.3) to treat uniformly rather than
// needing a caps-specific case.
class caps : public containers::object {
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

inline bool caps::is_compatible_with(const caps &other) const {
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

inline caps caps::intersect(const caps &other) const {
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

} // namespace cxflow
