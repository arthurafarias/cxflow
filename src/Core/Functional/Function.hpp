// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <functional>
#include <tuple>

namespace Core::Functional {
template <typename... ArgsTypes>
class Function : public std::function<ArgsTypes...> {
public:
  using std::function<ArgsTypes...>::function;
};
} // namespace Core::Functional