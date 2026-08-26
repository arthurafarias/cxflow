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

// SRS-004 §5.1: application-fed buffers - the embedding integration point
// for pushing externally-produced data into a pipeline. Unlike fake_src/
// file_src/fd_src, there is no internal task/thread: the embedding
// application drives push_buffer()/end_of_stream() directly on its own
// thread, exactly the way it would call any other public method - no
// polling, no callback the application has to implement first.
class app_src : public element {
public:
  explicit app_src(std::string name);

  flow_return push_buffer(buffer buf) { return src_pad_.push(std::move(buf)); }
  void end_of_stream() { src_pad_.send_event(event{event_type::eos}); }

  static void register_type();

private:
  pad &src_pad_;
};

inline app_src::app_src(std::string name)
    : element(std::move(name)), src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))) {}

inline void app_src::register_type() {
  element_factory::register_type("app_src",
                                  [](std::string name) { return std::make_shared<app_src>(std::move(name)); });
}

} // namespace cxflow::elements
