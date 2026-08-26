// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <string>

#include <cxflow/core/caps.hpp>
#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>

namespace cxflow::elements {

// SRS-004 §5.1: restricts/asserts caps between two elements. Unlike a
// generic `object` property (containers::variant's closed set has no caps
// alternative - the same reason pad::set_caps() is its own dedicated method,
// not a property), the filter caps are set via set_caps(), not
// property_set(). This codebase's caps model resolves compatibility once,
// at pad::link() time (no per-buffer caps renegotiation), so "restricting"
// is exactly "this element's pads advertise the configured caps instead of
// any(), so link() naturally rejects an incompatible neighbor" - the
// element itself is a plain passthrough once linked, same shape as
// identity.hpp.
class caps_filter : public element {
public:
  explicit caps_filter(std::string name);

  void set_caps(caps c) {
    sink_pad_.set_caps(c);
    src_pad_.set_caps(std::move(c));
  }

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  pad &sink_pad_;
  pad &src_pad_;
};

inline caps_filter::caps_filter(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline void caps_filter::register_type() {
  element_factory::register_type("caps_filter",
                                  [](std::string name) { return std::make_shared<caps_filter>(std::move(name)); });
}

inline flow_return caps_filter::chain(pad & /*sink_pad*/, buffer buf) { return src_pad_.push(std::move(buf)); }

inline bool caps_filter::handle_event(pad & /*sink_pad*/, const event &ev) { return src_pad_.send_event(ev); }

} // namespace cxflow::elements
