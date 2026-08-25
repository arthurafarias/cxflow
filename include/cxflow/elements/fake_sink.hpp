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

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/core/message.hpp>

namespace cxflow::elements {

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

inline fake_sink::fake_sink(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline void fake_sink::register_type() {
  element_factory::register_type("fake_sink",
                                  [](std::string name) { return std::make_shared<fake_sink>(std::move(name)); });
}

inline flow_return fake_sink::chain(pad & /*sink_pad*/, buffer /*buf*/) {
  ++buffers_received_;
  return flow_return::ok;
}

inline bool fake_sink::handle_event(pad & /*sink_pad*/, const event &ev) {
  if (ev.type == event_type::eos) {
    message msg;
    msg.type = message_type::eos;
    msg.source = shared_from_this();
    post_message(std::move(msg));
  }
  return true;
}

} // namespace cxflow::elements
