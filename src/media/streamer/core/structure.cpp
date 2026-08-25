// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include <media/streamer/core/structure.hpp>

namespace media::streamer {

bool structure::is_compatible_with(const structure &other) const {
  if (name_ != other.name_) {
    return false;
  }

  for (const auto &[field, value] : fields_) {
    if (const auto *other_value = other.get(field); other_value != nullptr && *other_value != value) {
      return false;
    }
  }

  return true;
}

} // namespace media::streamer
