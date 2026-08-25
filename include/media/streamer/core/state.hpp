// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

namespace media::streamer {

// Ordered null < ready < paused < playing - element::set_state() walks
// adjacent steps in this order, and bin propagates to children sink-first
// on the way up, source-first on the way down.
enum class state {
  null,
  ready,
  paused,
  playing,
};

enum class state_change_return {
  failure,
  success,
  async,
  no_preroll,
};

} // namespace media::streamer
