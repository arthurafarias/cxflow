// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once
#include "Modules/Serialization/Base/TagPart.hpp"
#include "Modules/Serialization/Base/TagType.hpp"
#include <Core/Containers/String.hpp>

using namespace CXORM::Core::Containers;

namespace CXORM::Serialization::Base {
struct TagBase {
  TagBase() {}
  TagBase(const String &name, const TagPart &part = TagPart::DoNotApply,
          const TagType &type = TagType::Integral)
      : name(name), part(part), type(type) {}
  String name = "";
  TagPart part = TagPart::DoNotApply;
  TagType type = TagType::Integral;
};
} // namespace CXORM::Serialization::Base