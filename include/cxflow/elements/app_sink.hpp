// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <deque>
#include <mutex>
#include <optional>
#include <string>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/core/message.hpp>
#include <cxflow/threading/signal.hpp>

namespace cxflow::elements {

// SRS-004 §5.1: application-consumed buffers - the embedding integration
// point for pulling pipeline output out to host application code. Offers
// both styles an embedder might want: try_pull_buffer() for a poll/pull
// consumer, and buffer_received/eos_received (this codebase's own
// signals-based notification model, SRS-001 §7.4) for a fully event-driven
// one - a consumer picks whichever fits, both are backed by the same
// internal queue.
class app_sink : public element {
public:
  explicit app_sink(std::string name);

  // nullopt if nothing has arrived yet.
  std::optional<buffer> try_pull_buffer();

  threading::signal<app_sink &, const buffer &> buffer_received;
  threading::signal<app_sink &> eos_received;

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  pad &sink_pad_;
  std::mutex mutex_;
  std::deque<buffer> queue_;
};

inline app_sink::app_sink(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline void app_sink::register_type() {
  element_factory::register_type("app_sink",
                                  [](std::string name) { return std::make_shared<app_sink>(std::move(name)); });
}

inline flow_return app_sink::chain(pad & /*sink_pad*/, buffer buf) {
  {
    std::unique_lock lock(mutex_);
    queue_.push_back(buf); // a plain copy shares buf's storage - buf itself is still valid below
  }
  buffer_received(*this, buf);
  return flow_return::ok;
}

inline bool app_sink::handle_event(pad & /*sink_pad*/, const event &ev) {
  if (ev.type == event_type::eos) {
    eos_received(*this);
    // Matches fake_sink's own convention - see file_sink.hpp's identical
    // comment. app_sink's eos_received signal is the pull/event-driven
    // consumer's own notification; the bus message is what lets a
    // surrounding pipeline (e.g. cxflow-launch) still detect completion
    // even when nothing is listening to eos_received directly.
    message msg;
    msg.type = message_type::eos;
    msg.source = weak_from_this();
    post_message(std::move(msg));
  }
  return true;
}

inline std::optional<buffer> app_sink::try_pull_buffer() {
  std::unique_lock lock(mutex_);
  if (queue_.empty()) {
    return std::nullopt;
  }
  buffer b = std::move(queue_.front());
  queue_.pop_front();
  return b;
}

} // namespace cxflow::elements
