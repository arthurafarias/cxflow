// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

namespace media::streamer {

// segment is deliberately omitted for now: nothing in the core or the
// default elements produces or consumes it yet (no seeking support). Add it
// back when seeking lands rather than modeling an unused tag today.
enum class event_type {
  flush_start,
  flush_stop,
  eos,
};

struct event {
  event_type type;
};

} // namespace media::streamer
