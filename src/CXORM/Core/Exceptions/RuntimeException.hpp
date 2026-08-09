// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <format>
#include <stdexcept>
#include <utility>

namespace CXORM::Core::Exceptions {
class RuntimeException : public std::runtime_error {
public:
  template <typename... ArgsTypes>
  RuntimeException(const std::format_string<ArgsTypes...> &fmt,
                  ArgsTypes &&...args)
      : std::runtime_error(
            std::format(fmt, std::forward<ArgsTypes>(args)...)) {}
};
} // namespace CXORM::Core::Exceptions