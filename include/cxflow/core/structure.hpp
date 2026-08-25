// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <variant>

namespace cxflow {

// A named set of typed fields, e.g. name "audio/x-raw" with fields like
// {"rate": 44100, "channels": 2}. No dynamic GValue-equivalent type system
// yet (deferred) - field values are one of a small fixed set of types.
class structure {
public:
  using field_value = std::variant<std::int64_t, double, std::string, bool>;

  structure() = default;
  explicit structure(std::string name) : name_(std::move(name)) {}

  const std::string &name() const { return name_; }

  void set(const std::string &field, field_value value) { fields_[field] = std::move(value); }
  const field_value *get(const std::string &field) const {
    auto it = fields_.find(field);
    return it == fields_.end() ? nullptr : &it->second;
  }

  // Same name required. A field present on both sides must be equal; a
  // field present on only one side does not constrain the match (subset
  // match, not exact-equality match).
  bool is_compatible_with(const structure &other) const;

private:
  std::string name_;
  std::map<std::string, field_value> fields_;
};

inline bool structure::is_compatible_with(const structure &other) const {
  if (name_ != other.name_) {
    return false;
  }

  for (const auto &[field, value] : fields_) {
    if (const auto *other_value = other.get(field); other_value != nullptr && *other_value != value) {
      return false;
    }
  }

  return true;
}

} // namespace cxflow
