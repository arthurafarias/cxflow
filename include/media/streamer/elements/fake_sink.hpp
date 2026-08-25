// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include <media/streamer/core/element.hpp>

namespace media::streamer::elements {

// A trivial sink: one sink pad, counts buffers received, and turns an
// incoming EOS event into an EOS bus message (only a sink posts that
// message; a source only ever pushes the event - see fake_src). Registered
// with element_factory under the type name "fake_sink".
class fake_sink : public element {
public:
  explicit fake_sink(std::string name);

  std::uint64_t buffers_received() const { return buffers_received_; }

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  pad &sink_pad_;
  std::atomic<std::uint64_t> buffers_received_{0};
};

} // namespace media::streamer::elements
