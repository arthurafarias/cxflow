// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#define UNUSED(x) ((void)(x))

namespace Utils {
struct Unused {
  template<typename ...UnusedTypes> 
  explicit Unused(const UnusedTypes &...) {}
  Unused(const Unused &) = delete;
  Unused &operator=(const Unused &) = delete;
  Unused(const Unused &&) = delete;
  Unused &operator=(const Unused &&) = delete;
};
} // namespace Utils