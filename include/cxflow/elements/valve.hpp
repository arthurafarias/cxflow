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

// SRS-004 §5.1: drops buffers when closed ("drop" property, default false -
// open). Matches GStreamer's valve semantics: a dropped buffer still
// returns flow_return::ok to the pusher (a closed valve is a deliberate,
// silent sink for data, not an upstream error condition). Events always
// pass through regardless of drop state - eos/flush must still reach
// downstream even while the valve is closed, the same way GStreamer's does.
class valve : public element {
public:
  explicit valve(std::string name);

  void set_drop(bool drop) { property_set("drop", drop); }
  bool is_dropping() const { return property_get<bool>("drop").value_or(false); }

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  pad &sink_pad_;
  pad &src_pad_;
};

inline valve::valve(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
  property_set("drop", false);
}

inline void valve::register_type() {
  element_factory::register_type("valve", [](std::string name) { return std::make_shared<valve>(std::move(name)); });
}

inline flow_return valve::chain(pad & /*sink_pad*/, buffer buf) {
  if (is_dropping()) {
    return flow_return::ok;
  }
  return src_pad_.push(std::move(buf));
}

inline bool valve::handle_event(pad & /*sink_pad*/, const event &ev) { return src_pad_.send_event(ev); }

} // namespace cxflow::elements
