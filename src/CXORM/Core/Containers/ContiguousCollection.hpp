// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include "CXORM/Core/Object.hpp"
#include <vector>
#include <functional>
#include <utility>

namespace CXORM::Core::Containers {
template <typename ContainedType>
class ContiguousCollection : public std::vector<ContainedType>, public Object {
public:
  template <typename... ArgsTypes>
  ContiguousCollection(const ArgsTypes&&... args)
      : std::vector<ContainedType>(std::forward<const ArgsTypes>(args)...) {}
  using std::vector<ContainedType>::vector;

  template <typename ReturnType>
  ContiguousCollection<ReturnType>
  transform(std::function<ReturnType(const ContainedType &)> transfomer) const {
    ContiguousCollection<ReturnType> retval;

    for (auto el : *this) {
      retval.push_back(transfomer(el));
    }

    return std::move(retval);
  }
};
} // namespace CXORM::Core::Containers