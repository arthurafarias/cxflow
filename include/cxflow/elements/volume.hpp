// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/elements/pcm_format.hpp>
#include <cxflow/logging/journal.hpp>

namespace cxflow::elements {

// SRS-004 §5.2: linear/dB gain applied to raw audio samples. The format
// (rate/channels/bits-per-sample/signed/float) is read from the sink pad's
// linked peer's caps the first time a buffer arrives, not from a property
// - the same resolution pattern wav_mux.hpp/au_mux.hpp use, since this
// codebase's caps model has no per-buffer negotiation and volume itself
// declares no caps of its own (any() on both pads, so it links to
// whatever's actually there).
class volume : public element {
public:
  explicit volume(std::string name);

  void set_level(double linear_gain) { property_set("level", linear_gain); }
  void set_gain_db(double decibels) { property_set("level", std::pow(10.0, decibels / 20.0)); }

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  double level() const { return property_get<double>("level").value_or(1.0); }
  std::optional<pcm_format> resolve_format() const;

  pad &sink_pad_;
  pad &src_pad_;
  std::optional<pcm_format> format_;
};

inline volume::volume(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
  property_set("level", 1.0);
}

inline void volume::register_type() {
  element_factory::register_type("volume",
                                  [](std::string name) { return std::make_shared<volume>(std::move(name)); });
}

inline std::optional<pcm_format> volume::resolve_format() const {
  if (pad *peer = sink_pad_.peer(); peer != nullptr) {
    return pcm_format_from_caps(peer->current_caps());
  }
  return std::nullopt;
}

inline flow_return volume::chain(pad & /*sink_pad*/, buffer buf) {
  if (!format_) {
    format_ = resolve_format();
    if (!format_) {
      journal::warn("volume '{}' has no resolvable audio/x-raw format on its sink pad's peer", name());
      return flow_return::error;
    }
    // Gain doesn't change the format - declare it on the src pad unchanged
    // so a downstream element resolving *its* format from this pad's caps
    // (audio_convert.hpp/wav_mux.hpp's own pattern) still sees it, instead
    // of any()'s default.
    src_pad_.set_caps(pcm_caps(*format_));
  }

  double gain = level();
  auto bytes_per_sample = format_->bytes_per_sample();
  auto src = buf.data();

  std::vector<std::byte> out_data(src.size());
  for (std::size_t i = 0; i + bytes_per_sample <= src.size(); i += bytes_per_sample) {
    double sample = read_pcm_sample(src.data() + i, *format_);
    write_pcm_sample(out_data.data() + i, *format_, sample * gain);
  }

  buffer out(std::move(out_data));
  out.pts = buf.pts;
  out.dts = buf.dts;
  out.duration = buf.duration;
  out.offset = buf.offset;

  return src_pad_.push(std::move(out));
}

inline bool volume::handle_event(pad & /*sink_pad*/, const event &ev) { return src_pad_.send_event(ev); }

} // namespace cxflow::elements
