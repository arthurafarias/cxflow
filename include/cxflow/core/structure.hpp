// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <optional>
#include <string>

#include <cxflow/containers/object.hpp>

namespace cxflow {

// A named set of typed fields, e.g. name "audio/x-raw" with fields like
// {"rate": 44100, "channels": 2}. No dynamic GValue-equivalent type system
// yet (deferred) - field values are one of a small fixed set of types.
//
// SRS-001 §5.4: storage is containers::object's map/signal rather than a
// hand-rolled std::map<std::string, field_value> - structure *is-a*
// observable variant_map, not a second, parallel map type. field_value is
// now a plain alias for containers::variant (rather than its own
// std::variant<int64_t, double, string, bool>) - the same closed set,
// generalized to the one the rest of the engine shares (§5.1, including
// OPEN-6's resolution: containers::variant gained a signed int64_t
// alternative specifically so a structure field's negative values keep
// round-tripping losslessly through this rebuild). set() is a thin wrapper
// over the inherited property_set(), so setting a field after construction
// now also fires property_changed (REQ-5.4.3) - a capability that falls out
// of inheriting object for free, not required to be consumed by anything in
// this pass.
class structure : public containers::object {
public:
  using field_value = containers::variant;

  structure() = default;
  explicit structure(std::string name) : name_(std::move(name)) {}

  const std::string &name() const { return name_; }

  // The only mutator (REQ-5.2.1's "set() is the only way to mutate an
  // entry" discipline, inherited from object/variant_map) - every field
  // write here necessarily notifies property_changed.
  void set(const std::string &field, field_value value) { property_set(field, std::move(value)); }

  // std::nullopt for an absent field, matching variant_map::get()'s
  // absence contract (§5.2) - not a raw pointer into internal storage
  // (there is none to point into: object's storage is snapshot-iterated
  // and mutex-guarded, not stable-addressed).
  std::optional<field_value> get(const std::string &field) const { return property_get_variant(field); }

  // Same name required. A field present on both sides must be equal; a
  // field present on only one side does not constrain the match (subset
  // match, not exact-equality match).
  bool is_compatible_with(const structure &other) const;

private:
  std::string name_;
};

inline bool structure::is_compatible_with(const structure &other) const {
  if (name_ != other.name_) {
    return false;
  }

  for (const auto &[field, value] : *this) {
    if (auto other_value = other.get(field); other_value.has_value() && *other_value != value) {
      return false;
    }
  }

  return true;
}

} // namespace cxflow
