// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include <media/streamer/elements/identity.hpp>

#include <media/streamer/core/element_factory.hpp>
#include <media/streamer/core/event.hpp>

namespace media::streamer::elements {

identity::identity(std::string name)
    : element(std::move(name)),
      sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

void identity::register_type() {
  element_factory::register_type("identity",
                                  [](std::string name) { return std::make_shared<identity>(std::move(name)); });
}

flow_return identity::chain(pad & /*sink_pad*/, buffer buf) { return src_pad_.push(std::move(buf)); }

bool identity::handle_event(pad & /*sink_pad*/, const event &ev) { return src_pad_.send_event(ev); }

} // namespace media::streamer::elements
