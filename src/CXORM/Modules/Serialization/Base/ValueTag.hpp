// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include "CXORM/Modules/Serialization/Base/KeyValueTag.hpp"
#include <CXORM/Core/Containers/String.hpp>


using namespace CXORM::Core::Containers;

namespace CXORM::Serialization::Base {

template <typename ValueType> struct ValueTag : KeyValueTag<ValueType> {
  ValueTag() {}
  ValueTag(const ValueType &value)
      : KeyValueTag<ValueType>(KeyValueTag<ValueType>::anonymous_name(),
                               value) {}
  virtual ~ValueTag() {}
};

} // namespace CXORM::Serialization::Base