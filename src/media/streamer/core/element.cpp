// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include <media/streamer/core/element.hpp>

namespace media::streamer {

namespace {

int rank(state s) { return static_cast<int>(s); }

state step_towards(state from, state target) {
  return static_cast<state>(rank(from) + (rank(target) > rank(from) ? 1 : -1));
}

} // namespace

pad &element::add_pad(std::unique_ptr<pad> new_pad) {
  std::unique_lock lock(pads_mutex_);
  pad &ref = *new_pad;
  pads_.push_back(std::move(new_pad));
  lock.unlock();

  pad_added(*this, ref);
  return ref;
}

pad *element::get_static_pad(const std::string &pad_name) const {
  std::unique_lock lock(pads_mutex_);
  for (const auto &p : pads_) {
    if (p->name() == pad_name) {
      return p.get();
    }
  }
  return nullptr;
}

state element::current_state() const {
  std::unique_lock lock(state_mutex_);
  return state_;
}

state_change_return element::set_state(state target) {
  state current = current_state();

  while (current != target) {
    state next = step_towards(current, target);
    state_change_return result = on_change_state(current, next);

    if (result == state_change_return::failure) {
      return state_change_return::failure;
    }

    {
      std::unique_lock lock(state_mutex_);
      state_ = next;
    }
    state_changed(*this, current, next);

    current = next;
  }

  return state_change_return::success;
}

state_change_return element::on_change_state(state from, state to) {
  std::unique_lock lock(pads_mutex_);

  if (from == state::ready && to == state::paused) {
    for (auto &p : pads_) {
      p->set_active(true);
    }
  } else if (from == state::paused && to == state::ready) {
    for (auto &p : pads_) {
      p->set_active(false);
    }
  }

  return state_change_return::success;
}

} // namespace media::streamer
