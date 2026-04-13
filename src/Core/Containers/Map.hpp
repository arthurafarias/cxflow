// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include "Core/Object.hpp"
#include <map>

namespace CXORM::Core::Containers {
template <typename... ArgsTypes>
class Map : public std::map<ArgsTypes...>, public Core::Object {
public:
  using std::map<ArgsTypes...>::map;

  template <typename... ConstructorArguments>
  Map(ConstructorArguments... args)
      : std::map<ArgsTypes...>(std::forward<ConstructorArguments>(args)...) {}

  template <typename... ConstructorArguments>
  Map(const ConstructorArguments &...args)
      : std::map<ArgsTypes...>(
            std::forward<const ConstructorArguments &>(args)...) {}

  template <typename... ConstructorArguments>
  Map(const ConstructorArguments &&...args)
      : std::map<ArgsTypes...>(
            std::forward<const ConstructorArguments &&>(args)...) {}
};
} // namespace CXORM::Core::Containers