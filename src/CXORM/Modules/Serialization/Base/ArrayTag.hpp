// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include "CXORM/Modules/Serialization/Base/TagBase.hpp"
#include "CXORM/Modules/Serialization/Base/TagPart.hpp"

namespace CXORM::Serialization::Base {
struct ArrayTag : public TagBase {
  ArrayTag(const String &name, const TagPart& part) : TagBase(name, part) {}
};
} // namespace CXORM::Serialization::Base