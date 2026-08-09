// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <ostream>

namespace Utils {
template <typename ArgumentType>
std::ostream &print(std::ostream &os, ArgumentType workspace);

template <>
inline std::ostream &print<const char *>(std::ostream &os, const char *value) {
  os << value;
  return os;
}
} // namespace Utils::Print