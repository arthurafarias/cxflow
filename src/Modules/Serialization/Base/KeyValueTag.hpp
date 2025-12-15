// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <Modules/Serialization/Base/TagBase.hpp>

#include <format>

namespace Modules::Serialization::Base {
template <typename ValueType> struct KeyValueTag : TagBase {
  KeyValueTag() {}
  KeyValueTag(const String &name, ValueType* value)
      : TagBase(name), value(value) {}
  ValueType* value;
  static inline String anonymous_name() {
    static int index = 0;
    return std::format("anonymous-{}", index++);
  }
};
} // namespace Modules::Serialization::Base