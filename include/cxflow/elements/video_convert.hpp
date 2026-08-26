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
#include <cxflow/elements/video_format.hpp>
#include <cxflow/logging/journal.hpp>

namespace cxflow::elements {

// SRS-004 §5.2: colorspace (YUV<->RGB) and chroma subsampling conversion,
// RGB24<->I420 (video_format.hpp's two supported pixel formats). Same
// input-resolved/output-explicit-with-passthrough-default pattern as
// audio_convert.hpp - see its class comment for why (no downstream
// negotiation to read an output preference from in this codebase's caps
// model). Width/height/framerate are carried through unchanged - this
// entry only ever changes the pixel format, never the frame geometry
// (that is video_scale's job).
class video_convert : public element {
public:
  explicit video_convert(std::string name);

  void set_output_format(pixel_format fmt) { output_format_ = fmt; }

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  std::optional<video_format> resolve_input_format() const;

  pad &sink_pad_;
  pad &src_pad_;
  std::optional<video_format> input_format_;
  std::optional<pixel_format> output_format_;
};

inline video_convert::video_convert(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline void video_convert::register_type() {
  element_factory::register_type(
      "video_convert", [](std::string name) { return std::make_shared<video_convert>(std::move(name)); });
}

inline std::optional<video_format> video_convert::resolve_input_format() const {
  if (pad *peer = sink_pad_.peer(); peer != nullptr) {
    return video_format_from_caps(peer->current_caps());
  }
  return std::nullopt;
}

inline flow_return video_convert::chain(pad & /*sink_pad*/, buffer buf) {
  if (!input_format_) {
    input_format_ = resolve_input_format();
    if (!input_format_) {
      journal::warn("video_convert '{}' has no resolvable video/x-raw format on its sink pad's peer", name());
      return flow_return::error;
    }
  }
  const video_format &in_fmt = *input_format_;
  pixel_format out_pixel_format = output_format_.value_or(in_fmt.format);

  video_format out_fmt = in_fmt;
  out_fmt.format = out_pixel_format;
  src_pad_.set_caps(video_caps(out_fmt));

  if (out_pixel_format == in_fmt.format) {
    return src_pad_.push(std::move(buf));
  }

  auto src = buf.data();
  std::vector<std::byte> in_data(src.begin(), src.end());
  std::vector<std::byte> out_data;
  if (in_fmt.format == pixel_format::rgb24 && out_pixel_format == pixel_format::i420) {
    out_data = rgb24_to_i420(in_data, in_fmt.width, in_fmt.height);
  } else if (in_fmt.format == pixel_format::i420 && out_pixel_format == pixel_format::rgb24) {
    out_data = i420_to_rgb24(in_data, in_fmt.width, in_fmt.height);
  } else {
    journal::warn("video_convert '{}' has no conversion path between the requested pixel formats", name());
    return flow_return::error;
  }

  buffer out(std::move(out_data));
  out.pts = buf.pts;
  out.dts = buf.dts;
  out.duration = buf.duration;
  out.offset = buf.offset;

  return src_pad_.push(std::move(out));
}

inline bool video_convert::handle_event(pad & /*sink_pad*/, const event &ev) { return src_pad_.send_event(ev); }

} // namespace cxflow::elements
