// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/core/message.hpp>
#include <cxflow/logging/journal.hpp>
#include <cxflow/threading/task.hpp>

namespace cxflow::elements {

// SRS-004 §5.1: reads an already-open file descriptor ("fd" property, an
// int64_t so a caller can still pass -1/"unset" as a recognizable sentinel,
// mirroring fake_src's own "num-buffers" convention) into buffers, on the
// same task-per-source shape as file_src. Deliberately does not close the
// fd itself on any transition - the fd is caller-owned (same contract as
// GStreamer's fdsrc), this element only ever read()s from it.
class fd_src : public element {
public:
  explicit fd_src(std::string name);

  void set_fd(int descriptor) { property_set("fd", static_cast<std::int64_t>(descriptor)); }
  void set_blocksize(std::uint64_t bytes) { property_set("blocksize", bytes); }

  static void register_type();

protected:
  state_change_return on_change_state(state from, state to) override;

private:
  void push_loop();

  int fd() const { return static_cast<int>(property_get<std::int64_t>("fd").value_or(-1)); }
  std::uint64_t blocksize() const { return property_get<std::uint64_t>("blocksize").value_or(4096); }

  pad &src_pad_;
  threading::task task_;
  std::uint64_t offset_ = 0;
};

inline fd_src::fd_src(std::string name)
    : element(std::move(name)), src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))),
      task_([this] { push_loop(); }) {
  property_set("fd", std::int64_t{-1});
  property_set("blocksize", std::uint64_t{4096});
}

inline void fd_src::register_type() {
  element_factory::register_type("fd_src", [](std::string name) { return std::make_shared<fd_src>(std::move(name)); });
}

inline state_change_return fd_src::on_change_state(state from, state to) {
  state_change_return result = element::on_change_state(from, to);
  if (result == state_change_return::failure) {
    return result;
  }

  if (from == state::paused && to == state::playing) {
    if (!task_.is_running()) {
      offset_ = 0;
      task_.start();
    } else {
      task_.resume();
    }
  } else if (from == state::playing && to == state::paused) {
    task_.pause();
  } else if (from == state::ready && to == state::null) {
    task_.stop();
  }

  return state_change_return::success;
}

inline void fd_src::push_loop() {
  int descriptor = fd();
  if (descriptor < 0) {
    journal::warn("fd_src '{}' has no fd set, sending eos", name());
    src_pad_.send_event(event{event_type::eos});
    task_.pause();
    return;
  }

  std::vector<std::byte> data(blocksize());
  ssize_t bytes_read = ::read(descriptor, data.data(), data.size());

  if (bytes_read < 0) {
    journal::warn("fd_src '{}' read() failed: {}", name(), std::strerror(errno));
    message msg;
    msg.type = message_type::error;
    msg.source = weak_from_this();
    msg.debug_info = "fd_src '" + name() + "': read() failed";
    post_message(std::move(msg));
    task_.pause();
    return;
  }
  if (bytes_read == 0) {
    src_pad_.send_event(event{event_type::eos});
    task_.pause();
    return;
  }

  data.resize(static_cast<std::size_t>(bytes_read));
  buffer buf(std::move(data));
  buf.offset = offset_;
  offset_ += static_cast<std::uint64_t>(bytes_read);

  src_pad_.push(std::move(buf));
}

} // namespace cxflow::elements
