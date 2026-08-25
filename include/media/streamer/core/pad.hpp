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

#include <media/streamer/core/buffer.hpp>
#include <media/streamer/core/caps.hpp>
#include <media/streamer/core/event.hpp>
#include <media/streamer/core/flow_return.hpp>
#include <media/streamer/threading/signal.hpp>

namespace media::streamer {

class element;

// Owned by its element (see element::add_pad); handed out elsewhere as a
// non-owning pad*/pad&. peer_ is likewise a non-owning raw pointer, cleared
// by unlink() - a deliberate simplification vs. GStreamer's independently
// refcounted pads, acceptable since dynamic pad removal / ghost pads are
// out of scope for this pass.
class pad {
public:
  enum class direction { src, sink };

  using chain_function = std::function<flow_return(pad &, buffer)>;
  using event_function = std::function<bool(pad &, const event &)>;

  pad(std::string name, direction dir, element &owner)
      : name_(std::move(name)), direction_(dir), owner_(owner), caps_(caps::any()) {}

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
  // boundary.
  void set_active(bool is_active) { active_ = is_active; }
  bool is_active() const { return active_; }

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
  bool active_ = false;
  bool flushing_ = false;
};

} // namespace media::streamer
