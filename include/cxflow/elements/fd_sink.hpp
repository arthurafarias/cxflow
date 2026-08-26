// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>

#include <unistd.h>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/core/message.hpp>
#include <cxflow/logging/journal.hpp>

namespace cxflow::elements {

// SRS-004 §5.1: writes buffers to an already-open file descriptor ("fd"
// property). Same caller-owned-fd contract as fd_src: never closes it.
// write() can legitimately return a short count for a pipe/socket fd, so
// chain() loops until every byte is written or write() itself fails.
class fd_sink : public element {
public:
  explicit fd_sink(std::string name);

  void set_fd(int descriptor) { property_set("fd", static_cast<std::int64_t>(descriptor)); }

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  int fd() const { return static_cast<int>(property_get<std::int64_t>("fd").value_or(-1)); }

  pad &sink_pad_;
};

inline fd_sink::fd_sink(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
  property_set("fd", std::int64_t{-1});
}

inline void fd_sink::register_type() {
  element_factory::register_type("fd_sink",
                                  [](std::string name) { return std::make_shared<fd_sink>(std::move(name)); });
}

inline flow_return fd_sink::chain(pad & /*sink_pad*/, buffer buf) {
  int descriptor = fd();
  if (descriptor < 0) {
    journal::warn("fd_sink '{}' has no fd set", name());
    return flow_return::error;
  }

  auto data = buf.data();
  std::size_t written = 0;
  while (written < data.size()) {
    ssize_t n = ::write(descriptor, data.data() + written, data.size() - written);
    if (n < 0) {
      journal::warn("fd_sink '{}' write() failed: {}", name(), std::strerror(errno));
      return flow_return::error;
    }
    written += static_cast<std::size_t>(n);
  }

  return flow_return::ok;
}

inline bool fd_sink::handle_event(pad & /*sink_pad*/, const event &ev) {
  if (ev.type == event_type::eos) {
    // Matches fake_sink's own convention - see file_sink.hpp's identical
    // comment for why this is required, not optional.
    message msg;
    msg.type = message_type::eos;
    msg.source = weak_from_this();
    post_message(std::move(msg));
  }
  return true;
}

} // namespace cxflow::elements
