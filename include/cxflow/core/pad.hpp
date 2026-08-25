// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <functional>
#include <string>

#include <cxflow/containers/object.hpp>
#include <cxflow/core/buffer.hpp>
#include <cxflow/core/caps.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/core/flow_return.hpp>
#include <cxflow/threading/signal.hpp>

namespace cxflow {

class element;

// Owned by its element (see element::add_pad); handed out elsewhere as a
// non-owning pad*/pad&. peer_ is likewise a non-owning raw pointer, cleared
// by unlink() - a deliberate simplification vs. GStreamer's independently
// refcounted pads, acceptable since dynamic pad removal / ghost pads are
// out of scope for this pass.
//
// SRS-001 §5.5: inherits containers::object for its scalar observable
// properties ("active" today, any future scalar pad property tomorrow) -
// set_active()/is_active() are thin wrappers over the inherited
// property_set()/property_get(), so a pad's activation state is now
// necessarily notified via property_changed. caps_ deliberately stays a
// plain typed `caps` member rather than becoming a variant-valued property
// (OPEN-1, resolved: caps is itself a containers::object subclass with its
// own independent property storage/observability, §5.4 - shoving a caps
// value into pad's own scalar property map would need variant to grow a
// non-scalar caps alternative, which REQ-5.1.1 rejects). flushing_ stays a
// plain, un-notified internal field per the baseline table (§4) - nothing
// outside receive_event()/receive() observes it.
//
// active_ additionally stays a plain mirrored bool, read-only outside
// set_active(): NFR-5 requires push()/receive() - the per-buffer hot path -
// byte-for-byte unmodified, which rules out receive() taking object's mutex
// and doing a map lookup (is_active()'s cost) on every single buffer.
// set_active() keeps both in lockstep; is_active() reads the property (the
// canonical, observable value), receive() reads the plain field (the
// zero-overhead one).
class pad : public containers::object {
public:
  enum class direction { src, sink };

  using chain_function = std::function<flow_return(pad &, buffer)>;
  using event_function = std::function<bool(pad &, const event &)>;

  pad(std::string name, direction dir, element &owner)
      : name_(std::move(name)), direction_(dir), owner_(owner), caps_(caps::any()) {
    property_set("active", false);
  }

  pad(const pad &) = delete;
  pad &operator=(const pad &) = delete;
  pad(pad &&) = delete;
  pad &operator=(pad &&) = delete;

  const std::string &name() const { return name_; }
  direction dir() const { return direction_; }
  element &owner() const { return owner_; }

  void set_chain_function(chain_function fn) { chain_ = std::move(fn); }
  void set_event_function(event_function fn) { event_fn_ = std::move(fn); }

  void set_caps(caps c) { caps_ = std::move(c); }
  const caps &current_caps() const { return caps_; }

  // Links this pad to sink_pad: fails if either side already has a peer,
  // the direction pairing isn't src->sink, or caps are incompatible.
  bool link(pad &sink_pad);
  void unlink();

  pad *peer() const { return peer_; }
  bool is_linked() const { return peer_ != nullptr; }

  // Driven by the owning element's on_change_state, at the ready<->paused
  // boundary. Backed by the inherited object's property storage (§5.5), so
  // every activation change is necessarily notified via property_changed -
  // see active_'s own comment for why receive() still reads a plain field
  // instead of going through is_active() itself.
  void set_active(bool is_active) {
    property_set("active", is_active);
    active_ = is_active;
  }
  bool is_active() const { return property_get<bool>("active").value_or(false); }

  // Pushes to this pad's peer (conceptually: called on a src pad to send
  // data downstream). Returns not_linked with no peer.
  flow_return push(buffer buf);

  // Sends ev to this pad's peer, which dispatches it locally via
  // receive_event (toggling its own flushing state for flush_start/stop,
  // then invoking its event_function if any). Returns false with no peer.
  bool send_event(const event &ev);

  threading::signal<pad &, const buffer &> buffer_probe;

private:
  friend class element;

  // Invoked on the *receiving* pad by its peer's push()/send_event().
  flow_return receive(buffer buf);
  bool receive_event(const event &ev);

  std::string name_;
  direction direction_;
  element &owner_;

  chain_function chain_;
  event_function event_fn_;
  caps caps_;

  pad *peer_ = nullptr;
  bool active_ = false; // hot-path mirror of the "active" property - see class comment
  bool flushing_ = false;
};

inline bool pad::link(pad &sink_pad) {
  if (direction_ != direction::src || sink_pad.direction_ != direction::sink) {
    return false;
  }
  if (is_linked() || sink_pad.is_linked()) {
    return false;
  }
  if (!caps_.is_compatible_with(sink_pad.caps_)) {
    return false;
  }

  peer_ = &sink_pad;
  sink_pad.peer_ = this;
  return true;
}

inline void pad::unlink() {
  if (peer_ != nullptr) {
    peer_->peer_ = nullptr;
    peer_ = nullptr;
  }
}

inline flow_return pad::push(buffer buf) {
  if (peer_ == nullptr) {
    return flow_return::not_linked;
  }

  buffer_probe(*this, buf);

  return peer_->receive(std::move(buf));
}

inline flow_return pad::receive(buffer buf) {
  if (!active_ || flushing_) {
    return flow_return::flushing;
  }
  if (!chain_) {
    return flow_return::error;
  }
  return chain_(*this, std::move(buf));
}

inline bool pad::send_event(const event &ev) {
  if (peer_ == nullptr) {
    return false;
  }
  return peer_->receive_event(ev);
}

inline bool pad::receive_event(const event &ev) {
  if (ev.type == event_type::flush_start) {
    flushing_ = true;
  } else if (ev.type == event_type::flush_stop) {
    flushing_ = false;
  }

  return event_fn_ ? event_fn_(*this, ev) : true;
}

} // namespace cxflow
