// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <memory>
#include <vector>

#include <media/streamer/core/element.hpp>

namespace media::streamer {

// Composite element: owns child elements and propagates state changes to
// them in data-flow order - sink-first on the way up (a sink must already
// be ready to receive before anything upstream is allowed to push, which is
// what PAUSED's preroll handshake requires), source-first on the way down
// (a source's push thread must stop before the sink it feeds is torn down).
// No ghost pads / external pads yet (deferred) - a bin has no pads of its
// own in this pass.
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

} // namespace media::streamer
