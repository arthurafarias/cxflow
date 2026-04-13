// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include "Core/SharedPointer.hpp"
#include <initializer_list>
#include <memory>
#include <utility>
namespace CXORM::Utils::Patterns {

template <typename DerivedType> class InitializerListCreator {
public:
  template <typename InitializerType>
  static SharedPointer<DerivedType>
  create(const std::initializer_list<InitializerType> &list) {
    return SharedPointer<DerivedType>(
        new DerivedType(list.begin(), list.end()));
  }
};

template <typename DerivedType, typename AbstractType = DerivedType> class Creator {
public:
  template <typename... DerivedConstructorArgumentsTypes>
  static SharedPointer<AbstractType>
  create(DerivedConstructorArgumentsTypes... args) {
    return std::dynamic_pointer_cast<AbstractType>(SharedPointer<DerivedType>(new DerivedType(
        std::forward<DerivedConstructorArgumentsTypes>(args)...)));
  }
};
} // namespace CXORM::Utils::Patterns