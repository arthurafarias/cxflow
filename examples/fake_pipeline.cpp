// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

// Builds fake_src ! identity ! fake_sink, runs it to EOS, and reports how
// many buffers the sink saw - the concrete end-to-end proof that pad
// linking, task-driven push scheduling, state propagation, and the
// bus/event/message split all work together, not just in isolation.

#include <chrono>
#include <iostream>

#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/message.hpp>
#include <cxflow/core/pad.hpp>
#include <cxflow/core/pipeline.hpp>
#include <cxflow/core/state.hpp>
#include <cxflow/elements/fake_sink.hpp>
#include <cxflow/elements/fake_src.hpp>
#include <cxflow/elements/identity.hpp>

using namespace cxflow;
using namespace cxflow::elements;

int main() {
  fake_src::register_type();
  identity::register_type();
  fake_sink::register_type();

  pipeline pipe("fake-pipeline");

  auto src = element_factory::create("fake_src", "src");
  auto id = element_factory::create("identity", "id");
  auto sink = element_factory::create("fake_sink", "sink");

  auto *src_impl = static_cast<fake_src *>(src.get());
  src_impl->set_num_buffers(10);
  src_impl->set_interval(std::chrono::milliseconds(20));

  pipe.add(src);
  pipe.add(id);
  pipe.add(sink);

  if (!src->get_static_pad("src")->link(*id->get_static_pad("sink")) ||
      !id->get_static_pad("src")->link(*sink->get_static_pad("sink"))) {
    std::cerr << "failed to link pipeline\n";
    return 1;
  }

  pipe.set_state(state::playing);

  bool running = true;
  while (running) {
    auto msg = pipe.bus().pop(std::chrono::milliseconds(500));
    if (!msg.has_value()) {
      continue;
    }

    switch (msg->type) {
    case message_type::eos:
      std::cout << "EOS received\n";
      running = false;
      break;
    case message_type::error:
      std::cerr << "error: " << msg->debug_info << "\n";
      running = false;
      break;
    default:
      break;
    }
  }

  pipe.set_state(state::null);

  auto *sink_impl = static_cast<fake_sink *>(sink.get());
  std::cout << "buffers received: " << sink_impl->buffers_received() << "\n";

  return 0;
}
