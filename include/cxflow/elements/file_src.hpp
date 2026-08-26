// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/core/message.hpp>
#include <cxflow/logging/journal.hpp>
#include <cxflow/threading/task.hpp>

namespace cxflow::elements {

// SRS-004 §5.1: reads a file into buffers. Opens "location" at
// null->ready (mirroring GStreamer's own "the file must exist by READY"
// contract) - a missing/unreadable file fails that state transition and
// posts a bus error, rather than failing silently once playing starts.
// Reading itself happens on a dedicated push task at paused->playing,
// exactly fake_src's own task-per-source shape ("blocksize" bytes per
// buffer, "location"'s running byte offset as buffer::offset).
class file_src : public element {
public:
  explicit file_src(std::string name);

  void set_location(std::string path) { property_set("location", std::move(path)); }
  void set_blocksize(std::uint64_t bytes) { property_set("blocksize", bytes); }

  static void register_type();

protected:
  state_change_return on_change_state(state from, state to) override;

private:
  void push_loop();

  std::string location() const { return property_get<std::string>("location").value_or(""); }
  std::uint64_t blocksize() const { return property_get<std::uint64_t>("blocksize").value_or(4096); }

  void post_open_error();

  pad &src_pad_;
  threading::task task_;
  std::ifstream file_;
  std::uint64_t offset_ = 0;
};

inline file_src::file_src(std::string name)
    : element(std::move(name)), src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))),
      task_([this] { push_loop(); }) {
  property_set("blocksize", std::uint64_t{4096});
}

inline void file_src::register_type() {
  element_factory::register_type("file_src",
                                  [](std::string name) { return std::make_shared<file_src>(std::move(name)); });
}

inline void file_src::post_open_error() {
  journal::warn("file_src '{}' failed to open '{}'", name(), location());
  message msg;
  msg.type = message_type::error;
  msg.source = weak_from_this();
  msg.debug_info = "file_src '" + name() + "': failed to open '" + location() + "'";
  post_message(std::move(msg));
}

inline state_change_return file_src::on_change_state(state from, state to) {
  if (from == state::null && to == state::ready) {
    file_.open(location(), std::ios::binary);
    if (!file_.is_open()) {
      post_open_error();
      return state_change_return::failure;
    }
  }

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
    if (file_.is_open()) {
      file_.close();
    }
  }

  return state_change_return::success;
}

inline void file_src::push_loop() {
  std::uint64_t block = blocksize();
  std::vector<std::byte> data(block);

  file_.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(block));
  std::streamsize bytes_read = file_.gcount();

  if (bytes_read <= 0) {
    src_pad_.send_event(event{event_type::eos});
    task_.pause(); // not stop(): stop() joins, and this runs on the task's own thread
    return;
  }

  data.resize(static_cast<std::size_t>(bytes_read));
  buffer buf(std::move(data));
  buf.offset = offset_;
  offset_ += static_cast<std::uint64_t>(bytes_read);

  src_pad_.push(std::move(buf));
}

} // namespace cxflow::elements
