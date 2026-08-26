// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

// SRS-003 §5.4: `cxflow-launch <description>` (§5.1's text grammar) or
// `cxflow-launch --file <path>` (§5.3's structured description, once §8
// OPEN-D1 picks a concrete on-disk format - not yet, see below).
//
// REQ-5.4.3: fully event-driven, no polling - connects to
// bus::message_posted before set_state(playing) and blocks the main thread
// on a condition variable signaled from that slot (eos/error) or SIGINT,
// exactly the pattern examples/cxflow-example-fake-pipeline.cpp already
// established for this codebase (§7.2 - one decided termination pattern,
// not two).

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <mutex>
#include <string>

#include <cxflow/core/bus.hpp>
#include <cxflow/core/message.hpp>
#include <cxflow/core/pipeline.hpp>
#include <cxflow/core/pipeline_parser.hpp>
#include <cxflow/core/state.hpp>
#include <cxflow/elements/fake_sink.hpp>
#include <cxflow/elements/fake_src.hpp>
#include <cxflow/elements/identity.hpp>
#include <cxflow/elements/wave1_plugins.hpp>

using namespace cxflow;

namespace {

std::mutex shutdown_mutex;
std::condition_variable shutdown_cv;
std::atomic<bool> interrupted{false};
std::atomic<bool> pipeline_finished{false};
std::atomic<int> exit_code{0};

// Async-signal-safe: only touches atomics and notifies a condition
// variable, matching examples/cxflow-example-fake-pipeline.cpp's own
// handler exactly.
extern "C" void handle_sigint(int /*signum*/) {
  interrupted.store(true);
  shutdown_cv.notify_all();
}

} // namespace

int main(int argc, char **argv) {
  // The reference catalog (SRS-002 §10 M4) until SRS-004 lands a real one -
  // registered unconditionally so a bare "fake_src ! identity ! fake_sink"
  // description resolves through element_factory (REQ-5.1.1) with no
  // separate catalog-loading step.
  elements::fake_src::register_type();
  elements::identity::register_type();
  elements::fake_sink::register_type();

  if (argc < 2) {
    std::cerr << "usage: cxflow-launch <description>\n"
                 "       cxflow-launch --file <path-to-structured-description>\n";
    return 1;
  }

  std::string arg1 = argv[1];

  if (arg1 == "--file") {
    // REQ-5.4.2/§8 OPEN-D1: no concrete on-disk variant<->format codec
    // exists yet (deferred to this SRS's M4) - only the variant tree shape
    // (§5.3) and the in-process to_variant()/from_variant() round trip are
    // implemented in this pass.
    std::cerr << "cxflow-launch: --file is not supported yet (SRS-003 OPEN-D1: no on-disk "
                 "format codec exists yet)\n";
    return 1;
  }

  auto parsed = pipeline_parser::parse(arg1);
  if (!parsed) {
    std::cerr << "cxflow-launch: parse error at byte " << parsed.error().position << ": " << parsed.error().message
               << "\n";
    return 1;
  }
  std::shared_ptr<pipeline> pipe = *parsed;

  // Every reaction is wired before set_state(playing) - REQ-5.4.3.
  pipe->bus().message_posted.connect([](bus &, const message &msg) {
    switch (msg.type) {
    case message_type::eos:
      exit_code.store(0);
      pipeline_finished.store(true);
      shutdown_cv.notify_all();
      break;
    case message_type::error:
      std::cerr << "cxflow-launch: error: " << msg.debug_info << "\n";
      exit_code.store(1);
      pipeline_finished.store(true);
      shutdown_cv.notify_all();
      break;
    default:
      break;
    }
  });

  std::signal(SIGINT, handle_sigint);

  if (pipe->set_state(state::playing) == state_change_return::failure) {
    std::cerr << "cxflow-launch: failed to reach the playing state\n";
    return 1;
  }

  {
    std::unique_lock lock(shutdown_mutex);
    shutdown_cv.wait(lock, [] { return interrupted.load() || pipeline_finished.load(); });
  }

  pipe->set_state(state::null);

  // REQ-5.4.4: 0 on clean eos, non-zero on a posted error; a SIGINT before
  // either (an unbounded pipeline) is a clean user-requested stop, not a
  // failure.
  return pipeline_finished.load() ? exit_code.load() : 0;
}
