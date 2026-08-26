// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <cxflow/core/element.hpp>
#include <cxflow/logging/journal.hpp>

namespace cxflow {

// Composite element: owns child elements and propagates state changes to
// them in data-flow order - sink-first on the way up (a sink must already
// be ready to receive before anything upstream is allowed to push, which is
// what PAUSED's preroll handshake requires), source-first on the way down
// (a source's push thread must stop before the sink it feeds is torn down).
// No ghost pads / external pads yet (deferred) - a bin has no pads of its
// own in this pass.
//
// SRS-001 §7.3: already inherits containers::object transitively through
// `element` (§5.6) - no separate/second inheritance is added here (that
// would create an ambiguous duplicate base). This already gives a bin its
// own observable property bag for free, the same way it already
// unconditionally carries pads_/state_/bus_ (OPEN-2); a future generic
// serialization walk (§7.3) that needs to also capture a bin's child/
// topology state (children_, walked via children()) is follow-on work, not
// part of this pass.
class bin : public element {
public:
  explicit bin(std::string name) : element(std::move(name)) {}

  void add(std::shared_ptr<element> child);
  void remove(const std::shared_ptr<element> &child);

  const std::vector<std::shared_ptr<element>> &children() const { return children_; }

protected:
  state_change_return on_change_state(state from, state to) override;

private:
  // Topological (source-first) order of children, built from the pad links
  // between them via Kahn's algorithm: a child fed by no other child in
  // this bin has in-degree 0 and is processed first. Children with no
  // relevant pad links fall back to insertion order.
  std::vector<std::shared_ptr<element>> topo_sorted_children() const;

  std::vector<std::shared_ptr<element>> children_;
};

inline void bin::add(std::shared_ptr<element> child) {
  child->set_bus(bus());
  journal::debug("bin '{}' added child '{}'", name(), child->name());
  children_.push_back(std::move(child));
}

inline void bin::remove(const std::shared_ptr<element> &child) {
  journal::debug("bin '{}' removed child '{}'", name(), child->name());
  children_.erase(std::remove(children_.begin(), children_.end(), child), children_.end());
}

inline std::vector<std::shared_ptr<element>> bin::topo_sorted_children() const {
  std::map<element *, int> in_degree;
  std::map<element *, std::vector<element *>> downstream_of;
  std::map<element *, std::shared_ptr<element>> owner_of;

  for (const auto &child : children_) {
    in_degree[child.get()] = 0;
    owner_of[child.get()] = child;
  }

  for (const auto &child : children_) {
    for (const auto &p : child->pads()) {
      if (p->dir() != pad::direction::src || !p->is_linked()) {
        continue;
      }
      element *downstream = &p->peer()->owner();
      if (!in_degree.contains(downstream)) {
        continue; // peer belongs to a different bin, or isn't a bin child
      }
      downstream_of[child.get()].push_back(downstream);
      in_degree[downstream]++;
    }
  }

  std::deque<element *> ready;
  for (const auto &child : children_) {
    if (in_degree[child.get()] == 0) {
      ready.push_back(child.get());
    }
  }

  std::vector<std::shared_ptr<element>> order;
  std::set<element *> visited;

  while (!ready.empty()) {
    element *current = ready.front();
    ready.pop_front();
    if (visited.contains(current)) {
      continue;
    }
    visited.insert(current);
    order.push_back(owner_of[current]);

    for (element *next : downstream_of[current]) {
      if (--in_degree[next] == 0) {
        ready.push_back(next);
      }
    }
  }

  // Anything unreached (a cycle, or genuinely isolated) is appended in
  // insertion order rather than dropped.
  for (const auto &child : children_) {
    if (!visited.contains(child.get())) {
      order.push_back(child);
    }
  }

  return order;
}

inline state_change_return bin::on_change_state(state from, state to) {
  auto order = topo_sorted_children(); // source-first

  bool upward = static_cast<int>(to) > static_cast<int>(from);
  if (upward) {
    std::reverse(order.begin(), order.end()); // sink-first
  }

  for (const auto &child : order) {
    if (child->set_state(to) == state_change_return::failure) {
      return state_change_return::failure;
    }
  }

  return state_change_return::success;
}

} // namespace cxflow
