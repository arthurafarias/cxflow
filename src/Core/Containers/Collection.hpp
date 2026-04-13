// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include "Core/Object.hpp"
#include <deque>
#include <functional>
#include <utility>

namespace CXORM::Core::Containers {
template <typename ContainedType>
class Collection : public std::deque<ContainedType>, public Object {
public:
  template <typename... ArgsTypes>
  Collection(const ArgsTypes&&... args)
      : std::deque<ContainedType>(std::forward<const ArgsTypes>(args)...) {}
  using std::deque<ContainedType>::deque;

  template <typename ReturnType>
  Collection<ReturnType>
  transform(std::function<ReturnType(const ContainedType &)> transfomer) const {
    Collection<ReturnType> retval;

    for (auto el : *this) {
      retval.push_back(transfomer(el));
    }

    return std::move(retval);
  }
};
} // namespace CXORM::Core::Containers