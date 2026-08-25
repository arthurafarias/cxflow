// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <string>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>

namespace cxflow::elements {

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

inline identity::identity(std::string name)
    : element(std::move(name)),
      sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline void identity::register_type() {
  element_factory::register_type("identity",
                                  [](std::string name) { return std::make_shared<identity>(std::move(name)); });
}

inline flow_return identity::chain(pad & /*sink_pad*/, buffer buf) { return src_pad_.push(std::move(buf)); }

inline bool identity::handle_event(pad & /*sink_pad*/, const event &ev) { return src_pad_.send_event(ev); }

} // namespace cxflow::elements
