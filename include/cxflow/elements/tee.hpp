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

// SRS-004 §5.1: one sink pad fans out to N src pads, each a "request pad"
// (GStreamer's term) added on demand via request_src_pad() rather than all
// declared upfront - element::add_pad() already supports being called after
// construction, so no new pad-request machinery is needed beyond that.
// buffer's storage is a shared_ptr to an immutable byte block (buffer.hpp),
// so fanning the same buffer out to every branch as plain copies (not
// buffer::copy()'s deep copy) is safe and cheap - every branch sees the
// same bytes, none of them can mutate what another branch already read.
class tee : public element {
public:
  explicit tee(std::string name);

  pad &request_src_pad();

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  pad &sink_pad_;
  int next_src_index_ = 0;
};

inline tee::tee(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline pad &tee::request_src_pad() {
  return add_pad(std::make_unique<pad>("src_" + std::to_string(next_src_index_++), pad::direction::src, *this));
}

inline void tee::register_type() {
  element_factory::register_type("tee", [](std::string name) { return std::make_shared<tee>(std::move(name)); });
}

inline flow_return tee::chain(pad & /*sink_pad*/, buffer buf) {
  bool any_linked = false;
  flow_return last_result = flow_return::not_linked;

  for (const auto &p : pads()) {
    if (p->dir() != pad::direction::src) {
      continue;
    }
    any_linked = true;
    last_result = p->push(buf); // plain copy: shares buf's underlying storage, see class comment
  }

  return any_linked ? last_result : flow_return::not_linked;
}

inline bool tee::handle_event(pad & /*sink_pad*/, const event &ev) {
  bool any_succeeded = false;
  for (const auto &p : pads()) {
    if (p->dir() != pad::direction::src) {
      continue;
    }
    any_succeeded = p->send_event(ev) || any_succeeded;
  }
  return any_succeeded;
}

} // namespace cxflow::elements
