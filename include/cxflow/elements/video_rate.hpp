// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/elements/video_format.hpp>
#include <cxflow/logging/journal.hpp>

namespace cxflow::elements {

// SRS-004 §5.2: frame duplication/drop to match a declared framerate - the
// per-frame counterpart of audio_rate.hpp's per-sample gap/overlap
// correction, same "first pts establishes time zero, everything after is
// measured in output-rate slots from there" shape. The output rate is
// explicit (set_output_output_framerate()), defaulting to the input
// format's own declared rate (a no-op) when unset - same reasoning as
// audio_resample.hpp's output rate.
//
// Each incoming frame maps to the nearest output "slot" (an integer
// multiple of the output frame period since the first frame). A frame
// landing on a slot already emitted (input arrived faster than the output
// rate needs) is dropped; every empty slot before a frame's own slot is
// filled by duplicating the most recently emitted frame (input arrived
// slower than the output rate needs) - real duplication/drop, not merely
// relabeling timestamps.
class video_rate : public element {
public:
  explicit video_rate(std::string name);

  void set_output_framerate(std::uint64_t numerator, std::uint64_t denominator) {
    output_framerate_num_ = numerator;
    output_framerate_den_ = denominator;
  }

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  std::optional<video_format> resolve_format() const;

  pad &sink_pad_;
  pad &src_pad_;
  std::optional<video_format> format_;
  std::optional<std::uint64_t> output_framerate_num_;
  std::optional<std::uint64_t> output_framerate_den_;

  std::optional<std::chrono::nanoseconds> base_pts_;
  std::int64_t next_slot_ = 0;
  std::optional<buffer> last_frame_;
};

inline video_rate::video_rate(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline void video_rate::register_type() {
  element_factory::register_type("video_rate",
                                  [](std::string name) { return std::make_shared<video_rate>(std::move(name)); });
}

inline std::optional<video_format> video_rate::resolve_format() const {
  if (pad *peer = sink_pad_.peer(); peer != nullptr) {
    return video_format_from_caps(peer->current_caps());
  }
  return std::nullopt;
}

inline flow_return video_rate::chain(pad & /*sink_pad*/, buffer buf) {
  if (!format_) {
    format_ = resolve_format();
    if (!format_) {
      journal::warn("video_rate '{}' has no resolvable video/x-raw format on its sink pad's peer", name());
      return flow_return::error;
    }
  }
  std::uint64_t out_num = output_framerate_num_.value_or(format_->framerate_num);
  std::uint64_t out_den = output_framerate_den_.value_or(format_->framerate_den);

  video_format out_fmt = *format_;
  out_fmt.framerate_num = out_num;
  out_fmt.framerate_den = out_den > 0 ? out_den : 1;
  src_pad_.set_caps(video_caps(out_fmt));

  if (out_num == 0 || !buf.pts.has_value()) {
    return src_pad_.push(std::move(buf)); // nothing to rate-correct against
  }
  double out_rate = static_cast<double>(out_num) / static_cast<double>(out_fmt.framerate_den);

  if (!base_pts_.has_value()) {
    base_pts_ = *buf.pts;
    next_slot_ = 0;
  }

  double elapsed = std::chrono::duration<double>(*buf.pts - *base_pts_).count();
  auto slot = static_cast<std::int64_t>(std::llround(elapsed * out_rate));

  if (slot < next_slot_) {
    // Superseded before it would ever be shown at this output rate.
    last_frame_ = buf;
    return flow_return::ok;
  }

  auto slot_pts = [&](std::int64_t s) {
    return *base_pts_ +
           std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(static_cast<double>(s) / out_rate));
  };

  flow_return result = flow_return::ok;
  while (next_slot_ < slot) {
    if (last_frame_.has_value()) {
      buffer dup = *last_frame_; // a plain copy shares storage - frame bytes are never mutated after generation
      dup.pts = slot_pts(next_slot_);
      result = src_pad_.push(std::move(dup));
    }
    ++next_slot_;
  }

  buffer out = buf;
  out.pts = slot_pts(next_slot_);
  last_frame_ = buf;
  result = src_pad_.push(std::move(out));
  ++next_slot_;

  return result;
}

inline bool video_rate::handle_event(pad & /*sink_pad*/, const event &ev) { return src_pad_.send_event(ev); }

} // namespace cxflow::elements
