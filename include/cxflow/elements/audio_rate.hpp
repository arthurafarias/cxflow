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
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/elements/pcm_format.hpp>
#include <cxflow/logging/journal.hpp>

namespace cxflow::elements {

// SRS-004 §5.2: timestamp-driven sample insertion/drop to match a declared
// rate. The format is auto-resolved from the sink pad's linked peer's
// caps (audio_convert's/volume's own pattern) - the "declared rate" this
// entry corrects *against* is exactly that format's own "rate" field, not
// a second, separately-configured target.
//
// The first buffer's pts establishes time zero; every buffer after that is
// compared against where its first frame *should* land given how many
// frames have already passed at the declared rate. A gap (upstream fell
// behind - actual pts later than expected) is filled with inserted
// silence; an overlap (actual pts earlier than expected) is corrected by
// dropping the overlapping leading frames. A buffer with no pts at all
// passes through unmodified - there is nothing to compare it against.
class audio_rate : public element {
public:
  explicit audio_rate(std::string name);

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  std::optional<pcm_format> resolve_format() const;

  pad &sink_pad_;
  pad &src_pad_;
  std::optional<pcm_format> format_;
  std::optional<std::chrono::nanoseconds> base_pts_;
  std::uint64_t frames_processed_ = 0;
};

inline audio_rate::audio_rate(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline void audio_rate::register_type() {
  element_factory::register_type("audio_rate",
                                  [](std::string name) { return std::make_shared<audio_rate>(std::move(name)); });
}

inline std::optional<pcm_format> audio_rate::resolve_format() const {
  if (pad *peer = sink_pad_.peer(); peer != nullptr) {
    return pcm_format_from_caps(peer->current_caps());
  }
  return std::nullopt;
}

inline flow_return audio_rate::chain(pad & /*sink_pad*/, buffer buf) {
  if (!format_) {
    format_ = resolve_format();
    if (!format_) {
      journal::warn("audio_rate '{}' has no resolvable audio/x-raw format on its sink pad's peer", name());
      return flow_return::error;
    }
    // Rate correction doesn't change the format - declare it on the src
    // pad unchanged so a downstream element resolving *its* format from
    // this pad's caps (volume.hpp/wav_mux.hpp's own pattern) still sees
    // it, instead of any()'s default.
    src_pad_.set_caps(pcm_caps(*format_));
  }
  const pcm_format &fmt = *format_;
  std::uint64_t frame_bytes = fmt.bytes_per_frame();

  if (frame_bytes == 0 || fmt.rate == 0 || !buf.pts.has_value()) {
    return src_pad_.push(std::move(buf)); // nothing to rate-correct against
  }

  if (!base_pts_.has_value()) {
    base_pts_ = *buf.pts;
    frames_processed_ = 0;
  }

  double rate = static_cast<double>(fmt.rate);
  std::chrono::nanoseconds expected_pts =
      *base_pts_ + std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::duration<double>(static_cast<double>(frames_processed_) / rate));

  double diff_seconds = std::chrono::duration<double>(*buf.pts - expected_pts).count();
  std::int64_t diff_frames = static_cast<std::int64_t>(std::llround(diff_seconds * rate));

  auto in_data = buf.data();
  std::uint64_t in_frame_count = in_data.size() / frame_bytes;
  std::uint64_t frames_to_drop = 0;

  std::vector<std::byte> out_data;
  if (diff_frames > 0) {
    std::uint64_t silence_frames = static_cast<std::uint64_t>(diff_frames);
    out_data.resize(silence_frames * frame_bytes);
    for (std::uint64_t f = 0; f < silence_frames; ++f) {
      for (std::uint64_t c = 0; c < fmt.channels; ++c) {
        write_pcm_sample(out_data.data() + (f * fmt.channels + c) * fmt.bytes_per_sample(), fmt, 0.0);
      }
    }
  } else if (diff_frames < 0) {
    frames_to_drop = std::min<std::uint64_t>(static_cast<std::uint64_t>(-diff_frames), in_frame_count);
  }

  std::size_t inserted_bytes = out_data.size();
  std::uint64_t kept_frames = in_frame_count - frames_to_drop;
  out_data.resize(inserted_bytes + kept_frames * frame_bytes);
  if (kept_frames > 0) {
    std::memcpy(out_data.data() + inserted_bytes, in_data.data() + frames_to_drop * frame_bytes,
                kept_frames * frame_bytes);
  }

  std::uint64_t total_frames = out_data.size() / frame_bytes;

  buffer out(std::move(out_data));
  out.pts = expected_pts;
  out.dts = buf.dts;
  out.offset = buf.offset;
  out.duration =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(total_frames / rate));

  frames_processed_ += total_frames;

  return src_pad_.push(std::move(out));
}

inline bool audio_rate::handle_event(pad & /*sink_pad*/, const event &ev) { return src_pad_.send_event(ev); }

} // namespace cxflow::elements
