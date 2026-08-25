// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include <media/streamer/elements/fake_sink.hpp>

#include <media/streamer/core/element_factory.hpp>
#include <media/streamer/core/event.hpp>
#include <media/streamer/core/message.hpp>

namespace media::streamer::elements {

fake_sink::fake_sink(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

void fake_sink::register_type() {
  element_factory::register_type("fake_sink",
                                  [](std::string name) { return std::make_shared<fake_sink>(std::move(name)); });
}

flow_return fake_sink::chain(pad & /*sink_pad*/, buffer /*buf*/) {
  ++buffers_received_;
  return flow_return::ok;
}

bool fake_sink::handle_event(pad & /*sink_pad*/, const event &ev) {
  if (ev.type == event_type::eos) {
    message msg;
    msg.type = message_type::eos;
    msg.source = shared_from_this();
    post_message(std::move(msg));
  }
  return true;
}

} // namespace media::streamer::elements
