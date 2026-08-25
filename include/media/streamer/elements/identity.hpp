// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <string>

#include <media/streamer/core/element.hpp>

namespace media::streamer::elements {

// Straight passthrough: one sink pad, one src pad, both default to any()
// caps (set by pad's own constructor). Its chain function simply re-pushes
// each buffer on the src pad; its event handler re-sends each event on the
// src pad. Proves 3-element linking (fake_src ! identity ! fake_sink).
// Registered with element_factory under the type name "identity".
class identity : public element {
public:
  explicit identity(std::string name);

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  pad &sink_pad_;
  pad &src_pad_;
};

} // namespace media::streamer::elements
