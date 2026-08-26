// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/elements/pcm_format.hpp>
#include <cxflow/logging/journal.hpp>

namespace cxflow::elements {

// SRS-004 §5.2: sample format (int16/int32/float) and channel-layout
// conversion. The input format is auto-resolved from the sink pad's linked
// peer's caps (volume's own pattern). The *output* format has no
// equivalent auto-resolution path in this codebase's caps model - nothing
// downstream of a filter element declares "the format I actually want"
// (wav_mux/file_sink/volume all keep any() pads too), so there is no
// negotiation to read from. set_output_format() is the explicit,
// caller-declared target; with none set, audio_convert passes the input
// format through unchanged - a graceful no-op default (matching
// caps_filter's own any()-by-default stance) rather than an error, since a
// text-grammar pipeline (SRS-003 §5.1) has no syntax to call it and should
// still link and run.
//
// Channel mapping beyond mono<->N is a simple index-wrap
// (`out[c] = in[c % input_channels]`), not a real downmix/upmix matrix -
// sufficient to prove the element works, not a mixing-console replacement.
class audio_convert : public element {
public:
  explicit audio_convert(std::string name);

  void set_output_format(pcm_format fmt) { output_format_ = fmt; }

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  std::optional<pcm_format> resolve_input_format() const;

  pad &sink_pad_;
  pad &src_pad_;
  std::optional<pcm_format> input_format_;
  std::optional<pcm_format> output_format_;
};

inline audio_convert::audio_convert(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline void audio_convert::register_type() {
  element_factory::register_type(
      "audio_convert", [](std::string name) { return std::make_shared<audio_convert>(std::move(name)); });
}

inline std::optional<pcm_format> audio_convert::resolve_input_format() const {
  if (pad *peer = sink_pad_.peer(); peer != nullptr) {
    return pcm_format_from_caps(peer->current_caps());
  }
  return std::nullopt;
}

inline flow_return audio_convert::chain(pad & /*sink_pad*/, buffer buf) {
  if (!input_format_) {
    input_format_ = resolve_input_format();
    if (!input_format_) {
      journal::warn("audio_convert '{}' has no resolvable audio/x-raw format on its sink pad's peer", name());
      return flow_return::error;
    }
    // Declare the (possibly converted) output format on the src pad so a
    // downstream element resolving *its* format from this pad's caps
    // (volume.hpp/wav_mux.hpp's own pattern) sees it, not any()'s default.
    src_pad_.set_caps(pcm_caps(output_format_.value_or(*input_format_)));
  }
  const pcm_format &in_fmt = *input_format_;
  const pcm_format &out_fmt = output_format_.value_or(in_fmt);

  auto src = buf.data();
  std::uint64_t in_frame_bytes = in_fmt.bytes_per_frame();
  if (in_frame_bytes == 0) {
    return flow_return::error;
  }
  std::uint64_t frame_count = src.size() / in_frame_bytes;

  std::vector<std::byte> out_data(frame_count * out_fmt.bytes_per_frame());

  for (std::uint64_t f = 0; f < frame_count; ++f) {
    const std::byte *in_frame = src.data() + f * in_frame_bytes;
    std::byte *out_frame = out_data.data() + f * out_fmt.bytes_per_frame();

    for (std::uint64_t c = 0; c < out_fmt.channels; ++c) {
      double value;
      if (out_fmt.channels == in_fmt.channels) {
        value = read_pcm_sample(in_frame + c * in_fmt.bytes_per_sample(), in_fmt);
      } else if (out_fmt.channels == 1) {
        double sum = 0.0;
        for (std::uint64_t ic = 0; ic < in_fmt.channels; ++ic) {
          sum += read_pcm_sample(in_frame + ic * in_fmt.bytes_per_sample(), in_fmt);
        }
        value = in_fmt.channels > 0 ? sum / static_cast<double>(in_fmt.channels) : 0.0;
      } else if (in_fmt.channels == 1) {
        value = read_pcm_sample(in_frame, in_fmt);
      } else {
        value = read_pcm_sample(in_frame + (c % in_fmt.channels) * in_fmt.bytes_per_sample(), in_fmt);
      }
      write_pcm_sample(out_frame + c * out_fmt.bytes_per_sample(), out_fmt, value);
    }
  }

  buffer out(std::move(out_data));
  out.pts = buf.pts;
  out.dts = buf.dts;
  out.duration = buf.duration;
  out.offset = buf.offset;

  return src_pad_.push(std::move(out));
}

inline bool audio_convert::handle_event(pad & /*sink_pad*/, const event &ev) { return src_pad_.send_event(ev); }

} // namespace cxflow::elements
