// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include "Core/Object.hpp"
#include "Core/Threading/UniqueLock.hpp"
#include <deque>

namespace CXORM::Core::Containers {
template <typename... ArgsTypes> class Tuple : public std::tuple<ArgsTypes...> {
public:
  using std::tuple<ArgsTypes...>::tuple;
};
} // namespace CXORM::Core::Containers