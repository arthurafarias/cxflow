// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <cxflow/containers/object.hpp>
#include <cxflow/core/bus.hpp>
#include <cxflow/core/pad.hpp>
#include <cxflow/core/state.hpp>
#include <cxflow/logging/journal.hpp>
#include <cxflow/threading/signal.hpp>

namespace cxflow {

// Owned via shared_ptr (required by element_factory::create, bin's children
// list, and message::source as a weak_ptr). Pads are owned by their
// element (vector<unique_ptr<pad>>), handed out as non-owning pad*/pad&.
//
// SRS-001 §5.6: inherits containers::object directly - custom element
// properties (fake_src's "num-buffers"/"interval-ms", any future element's
// tunables) go through the inherited property_set()/property_get(), so the
// factory/application layer can tune an element generically
// (element->property_set("num-buffers", std::int64_t{10})) without
// downcasting to the concrete element type or #include-ing its header
// (REQ-5.6.2). state_ deliberately stays outside that property map (§5.6.1,
// following OPEN-1's resolution): it is a plain enum with no variant
// alternative to hold it, notified through its own independent
// state_changed signal rather than being shoehorned into a variant-valued
// property entry.
class element : public std::enable_shared_from_this<element>, public containers::object {
public:
  explicit element(std::string name) : name_(std::move(name)) {}
  virtual ~element() = default;

  element(const element &) = delete;
  element &operator=(const element &) = delete;
  element(element &&) = delete;
  element &operator=(element &&) = delete;

  const std::string &name() const { return name_; }

  // SRS-003 §5.3 (REQ-5.3.1's "type" field): the element_factory type name
  // this instance was created under, stamped by element_factory::create()
  // (element_factory.hpp) - empty for an instance constructed directly
  // (e.g. `std::make_shared<element>(name)` in a test), since there is no
  // registered type name to record. This is the one piece of "what kind of
  // thing am I" element itself needs to hold for pipeline::to_variant() to
  // serialize a child generically, without a second, per-element-type
  // registry lookup keyed by instance identity.
  const std::string &registered_type_name() const { return registered_type_name_; }
  void set_registered_type_name(std::string type_name) { registered_type_name_ = std::move(type_name); }

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

  // Sugar over state_changed for the common case of "I don't need `self`,
  // just old/new" (SRS-001 §5.8's `pipe.on_state_changed([](state, state)
  // {...})` example) - a thin connect() wrapper, not a second notification
  // path.
  using state_changed_connection = threading::signal<element &, state, state>::connection;
  state_changed_connection on_state_changed(std::function<void(state, state)> fn) {
    return state_changed.connect([fn = std::move(fn)](element &, state old_state, state new_state) { fn(old_state, new_state); });
  }

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
  std::string registered_type_name_;

  mutable std::mutex pads_mutex_;
  std::vector<std::unique_ptr<pad>> pads_;

  mutable std::mutex state_mutex_;
  state state_ = state::null;

  mutable std::mutex bus_mutex_;
  std::shared_ptr<class bus> bus_;
};

namespace detail {

inline int element_state_rank(state s) { return static_cast<int>(s); }

inline state element_step_towards(state from, state target) {
  return static_cast<state>(element_state_rank(from) + (element_state_rank(target) > element_state_rank(from) ? 1 : -1));
}

} // namespace detail

inline pad &element::add_pad(std::unique_ptr<pad> new_pad) {
  std::unique_lock lock(pads_mutex_);
  pad &ref = *new_pad;
  pads_.push_back(std::move(new_pad));
  lock.unlock();

  journal::debug("element '{}' added pad '{}'", name_, ref.name());
  pad_added(*this, ref);
  return ref;
}

inline pad *element::get_static_pad(const std::string &pad_name) const {
  std::unique_lock lock(pads_mutex_);
  for (const auto &p : pads_) {
    if (p->name() == pad_name) {
      return p.get();
    }
  }
  return nullptr;
}

inline state element::current_state() const {
  std::unique_lock lock(state_mutex_);
  return state_;
}

inline state_change_return element::set_state(state target) {
  state current = current_state();

  while (current != target) {
    state next = detail::element_step_towards(current, target);
    journal::debug("element '{}' changing state {} -> {}", name_, to_string(current), to_string(next));
    state_change_return result = on_change_state(current, next);

    if (result == state_change_return::failure) {
      journal::warn("element '{}' failed to change state {} -> {}", name_, to_string(current), to_string(next));
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

inline state_change_return element::on_change_state(state from, state to) {
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

} // namespace cxflow
