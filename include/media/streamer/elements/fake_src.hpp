// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include <media/streamer/core/element.hpp>
#include <media/streamer/threading/task.hpp>

namespace media::streamer::elements {

// A trivial source: one src pad plus a dedicated task that generates and
// pushes buffers on its own thread while playing. Registered with
// element_factory under the type name "fake_src" (see register_types() in
// fake_src.cpp).
//
// Only ever pushes an EOS *event* downstream when done - never posts an EOS
// *message* itself. That is fake_sink's job, on receiving the event; real
// GStreamer/gstbasesink semantics.
class fake_src : public element {
public:
  explicit fake_src(std::string name);

  void set_num_buffers(std::int64_t count) { num_buffers_ = count; } // -1 = unbounded
  void set_interval(std::chrono::milliseconds interval) { interval_ = interval; }

  static void register_type();

protected:
  state_change_return on_change_state(state from, state to) override;

private:
  void push_loop();

  pad &src_pad_;
  threading::task task_;

  std::int64_t num_buffers_ = -1;
  std::chrono::milliseconds interval_{0};
  std::int64_t pushed_ = 0;
};

} // namespace media::streamer::elements
