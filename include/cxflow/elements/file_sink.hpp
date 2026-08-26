// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <fstream>
#include <string>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/core/message.hpp>
#include <cxflow/logging/journal.hpp>

namespace cxflow::elements {

// SRS-004 §5.1: writes buffers to a file. Opens "location" (truncating any
// existing content) at null->ready, same open-early contract as file_src;
// closed at ready->null. eos flushes explicitly rather than relying on the
// std::ofstream destructor, so every byte is durably on disk by the time
// the eos bus message (fake_sink's own pattern) would reach an application.
class file_sink : public element {
public:
  explicit file_sink(std::string name);

  void set_location(std::string path) { property_set("location", std::move(path)); }

  static void register_type();

protected:
  state_change_return on_change_state(state from, state to) override;

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  std::string location() const { return property_get<std::string>("location").value_or(""); }
  void post_open_error();

  pad &sink_pad_;
  std::ofstream file_;
};

inline file_sink::file_sink(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline void file_sink::register_type() {
  element_factory::register_type("file_sink",
                                  [](std::string name) { return std::make_shared<file_sink>(std::move(name)); });
}

inline void file_sink::post_open_error() {
  journal::warn("file_sink '{}' failed to open '{}'", name(), location());
  message msg;
  msg.type = message_type::error;
  msg.source = weak_from_this();
  msg.debug_info = "file_sink '" + name() + "': failed to open '" + location() + "'";
  post_message(std::move(msg));
}

inline state_change_return file_sink::on_change_state(state from, state to) {
  if (from == state::null && to == state::ready) {
    file_.open(location(), std::ios::binary | std::ios::trunc);
    if (!file_.is_open()) {
      post_open_error();
      return state_change_return::failure;
    }
  }

  state_change_return result = element::on_change_state(from, to);
  if (result == state_change_return::failure) {
    return result;
  }

  if (from == state::ready && to == state::null) {
    if (file_.is_open()) {
      file_.close();
    }
  }

  return state_change_return::success;
}

inline flow_return file_sink::chain(pad & /*sink_pad*/, buffer buf) {
  if (!file_.is_open()) {
    return flow_return::error;
  }
  auto data = buf.data();
  file_.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
  return file_.good() ? flow_return::ok : flow_return::error;
}

inline bool file_sink::handle_event(pad & /*sink_pad*/, const event &ev) {
  if (ev.type == event_type::eos) {
    if (file_.is_open()) {
      file_.flush();
    }
    // Matches fake_sink's own convention: only a sink turns an incoming
    // eos *event* into a posted eos bus *message* - real GStreamer/
    // gstbasesink semantics, and the only way an event-driven consumer
    // (cxflow-launch, SRS-003 REQ-5.4.3) learns the pipeline finished.
    message msg;
    msg.type = message_type::eos;
    msg.source = weak_from_this();
    post_message(std::move(msg));
  }
  return true;
}

} // namespace cxflow::elements
