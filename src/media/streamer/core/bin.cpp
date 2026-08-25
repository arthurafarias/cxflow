// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include <media/streamer/core/bin.hpp>

#include <algorithm>
#include <deque>
#include <map>
#include <set>

namespace media::streamer {

void bin::add(std::shared_ptr<element> child) {
  child->set_bus(bus());
  children_.push_back(std::move(child));
}

void bin::remove(const std::shared_ptr<element> &child) {
  children_.erase(std::remove(children_.begin(), children_.end(), child), children_.end());
}

std::vector<std::shared_ptr<element>> bin::topo_sorted_children() const {
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

state_change_return bin::on_change_state(state from, state to) {
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

} // namespace media::streamer
