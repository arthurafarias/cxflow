// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include <media/streamer/core/pad.hpp>

namespace media::streamer {

bool pad::link(pad &sink_pad) {
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

void pad::unlink() {
  if (peer_ != nullptr) {
    peer_->peer_ = nullptr;
    peer_ = nullptr;
  }
}

flow_return pad::push(buffer buf) {
  if (peer_ == nullptr) {
    return flow_return::not_linked;
  }

  buffer_probe(*this, buf);

  return peer_->receive(std::move(buf));
}

flow_return pad::receive(buffer buf) {
  if (!active_ || flushing_) {
    return flow_return::flushing;
  }
  if (!chain_) {
    return flow_return::error;
  }
  return chain_(*this, std::move(buf));
}

bool pad::send_event(const event &ev) {
  if (peer_ == nullptr) {
    return false;
  }
  return peer_->receive_event(ev);
}

bool pad::receive_event(const event &ev) {
  if (ev.type == event_type::flush_start) {
    flushing_ = true;
  } else if (ev.type == event_type::flush_stop) {
    flushing_ = false;
  }

  return event_fn_ ? event_fn_(*this, ev) : true;
}

} // namespace media::streamer
