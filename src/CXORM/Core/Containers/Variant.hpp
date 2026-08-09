// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <CXORM/Core/Containers/String.hpp>

#include <CXORM/Utils/Print.hpp>

#include <deque>
#include <iomanip>
#include <string>
#include <type_traits>
#include <variant>

using namespace CXORM::Core::Containers;

namespace CXORM::Core::Containers {
template <typename... ArgsTypes> class Variant : public std::variant<ArgsTypes...>, public Object {
public:
  using std::variant<ArgsTypes...>::variant;
  using base_type = std::variant<ArgsTypes...>;
};
} // namespace CXORM::Core::Containers