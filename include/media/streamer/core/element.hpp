// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <media/streamer/core/bus.hpp>
#include <media/streamer/core/pad.hpp>
#include <media/streamer/core/state.hpp>
#include <media/streamer/threading/signal.hpp>

namespace media::streamer {

// Owned via shared_ptr (required by element_factory::create, bin's children
// list, and message::source as a weak_ptr). Pads are owned by their
// element (vector<unique_ptr<pad>>), handed out as non-owning pad*/pad&.
class element : public std::enable_shared_from_this<element> {
public:
  explicit element(std::string name) : name_(std::move(name)) {}
  virtual ~element() = default;

  element(const element &) = delete;
  element &operator=(const element &) = delete;
  element(element &&) = delete;
  element &operator=(element &&) = delete;

  const std::string &name() const { return name_; }

  pad &add_pad(std::unique_ptr<pad> new_pad);
  pad *get_static_pad(const std::string &pad_name) const;
  const std::vector<std::unique_ptr<pad>> &pads() const { return pads_; }

  // Decomposes a (possibly multi-step) request into single adjacent-state
  // transitions, calling on_change_state once per step and firing
  // state_changed after each successful one. Stops and returns failure
  // immediately on the first failing step - already-applied steps are not
  // rolled back.
  state_change_return set_state(state target);
  state current_state() const;

  threading::signal<element &, state, state> state_changed; // (self, old, new)
  threading::signal<element &, pad &> pad_added;

  // Set by bin::add() when this element becomes (transitively) a pipeline's
  // child, mirroring how gst_element_post_message() walks up to the
  // pipeline's bus in real GStreamer - here it's pushed down instead of
  // walked up, since elements don't hold a parent pointer in this pass.
  void set_bus(std::shared_ptr<class bus> b) {
    std::unique_lock lock(bus_mutex_);
    bus_ = std::move(b);
  }
  std::shared_ptr<class bus> bus() const {
    std::unique_lock lock(bus_mutex_);
    return bus_;
  }
  void post_message(message msg) const {
    if (auto b = bus()) {
      b->post(std::move(msg));
    }
  }

protected:
  // Default implementation activates/deactivates every pad at the
  // ready<->paused boundary; override to add real behavior (task
  // start/stop, resource acquisition/release, ...), calling the base
  // implementation for that pad-activation behavior to still apply.
  virtual state_change_return on_change_state(state from, state to);

private:
  std::string name_;

  mutable std::mutex pads_mutex_;
  std::vector<std::unique_ptr<pad>> pads_;

  mutable std::mutex state_mutex_;
  state state_ = state::null;

  mutable std::mutex bus_mutex_;
  std::shared_ptr<class bus> bus_;
};

} // namespace media::streamer
