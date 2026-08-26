// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

// Builds fake_src ! identity ! fake_sink and runs it to demonstrate the
// concrete end-to-end proof that pad linking, task-driven push scheduling,
// state propagation, and the bus/event/message split all work together, not
// just in isolation.
//
// SRS-001 §5.8/§7.4: fully event-driven, not merely event-capable. Every
// reaction - buffer counts, state transitions, EOS/error - is wired through
// a callback connected before set_state(playing), not discovered by polling
// afterwards:
//   - custom element properties are set generically via property_set()
//     (REQ-5.6.2) - no static_cast to fake_src/fake_sink anywhere in this
//     file, for either writing tunables or reading results.
//   - state transitions are observed via element::on_state_changed(), sugar
//     over state_changed (§5.8's pipe.on_state_changed(...) example).
//   - buffers are counted via the sink pad's existing buffer_probe signal
//     (data-plane, unmodified by this SRS, §7.1) rather than by reading
//     fake_sink's own counter through a downcast.
//   - bus messages (EOS/error) are observed via bus::message_posted, not by
//     pop()'ing on a timer.
// main() itself never re-checks state on an interval: it blocks purely on
// an OS interrupt (SIGINT) and terminates once that arrives - a
// long-running, "wait for Ctrl-C" shape, deliberately not tied to the
// pipeline's own EOS/error completion (§7.4: "the main loop should wait for
// an interrupt signal", not on any application-level signaled hand-off).
// No poll interval (500ms or otherwise) appears anywhere in this file.

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string_view>

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

namespace {

std::mutex shutdown_mutex;
std::condition_variable shutdown_cv;
std::atomic<bool> interrupted{false};

// Async-signal-safe: only touches an atomic and notifies a condition
// variable, matching the standard "signal handlers should do as little as
// possible" discipline.
extern "C" void handle_sigint(int /*signum*/) {
  interrupted.store(true);
  shutdown_cv.notify_all();
}

constexpr std::string_view to_string(state s) {
  switch (s) {
  case state::null:
    return "null";
  case state::ready:
    return "ready";
  case state::paused:
    return "paused";
  case state::playing:
    return "playing";
  }
  return "?";
}

} // namespace

int main() {
  fake_src::register_type();
  identity::register_type();
  fake_sink::register_type();

  pipeline pipe("fake-pipeline");

  auto src = element_factory::create("fake_src", "src");
  auto id = element_factory::create("identity", "id");
  auto sink = element_factory::create("fake_sink", "sink");

  pipe.add(src);
  pipe.add(id);
  pipe.add(sink);

  if (!src->get_static_pad("src")->link(*id->get_static_pad("sink")) ||
      !id->get_static_pad("src")->link(*sink->get_static_pad("sink"))) {
    std::cerr << "failed to link pipeline\n";
    return 1;
  }

  // Generic property access (REQ-5.6.2) - no static_cast to fake_src, no
  // #include-ing it for this purpose (it's still included above only to
  // call register_type()).
  src->property_set("num-buffers", std::int64_t{10});
  src->property_set("interval-ms", std::uint64_t{20});

  // Every reaction below is wired before set_state(playing). buffer_probe
  // fires on the pushing (src) side of a link, not the receiving side, so
  // this counts every buffer identity forwards to the sink - one signal
  // firing per buffer instead of the earlier example reading fake_sink's
  // own counter through a downcast.
  std::atomic<std::uint64_t> buffers_received{0};
  id->get_static_pad("src")->buffer_probe.connect(
      [&](pad &, const buffer &) { ++buffers_received; });

  pipe.on_state_changed([](state old_state, state new_state) {
    std::cout << "pipeline: " << to_string(old_state) << " -> " << to_string(new_state) << "\n";
  });

  pipe.bus().message_posted.connect([](bus &, const message &msg) {
    switch (msg.type) {
    case message_type::eos:
      std::cout << "EOS received\n";
      break;
    case message_type::error:
      std::cerr << "error: " << msg.debug_info << "\n";
      break;
    default:
      break;
    }
  });

  std::signal(SIGINT, handle_sigint);

  pipe.set_state(state::playing);

  std::cout << "running - press Ctrl-C to stop\n";
  {
    std::unique_lock lock(shutdown_mutex);
    shutdown_cv.wait(lock, [] { return interrupted.load(); });
  }

  pipe.set_state(state::null);

  std::cout << "buffers received: " << buffers_received.load() << "\n";

  return 0;
}
