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

// SRS-004 §5.2: image resampling (bilinear), RGB24 and I420 both
// supported by calling video_format.hpp's generic bilinear_scale_plane()
// once for RGB24's single interleaved plane, or three times for I420 (Y at
// full resolution, U/V at chroma resolution) - one routine, not a
// per-format reimplementation. Same input-resolved/output-explicit pattern
// as video_convert.hpp/audio_convert.hpp.
class video_scale : public element {
public:
  explicit video_scale(std::string name);

  void set_output_size(std::uint64_t width, std::uint64_t height) {
    output_width_ = width;
    output_height_ = height;
  }

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  std::optional<video_format> resolve_input_format() const;

  pad &sink_pad_;
  pad &src_pad_;
  std::optional<video_format> input_format_;
  std::optional<std::uint64_t> output_width_;
  std::optional<std::uint64_t> output_height_;
};

inline video_scale::video_scale(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline void video_scale::register_type() {
  element_factory::register_type(
      "video_scale", [](std::string name) { return std::make_shared<video_scale>(std::move(name)); });
}

inline std::optional<video_format> video_scale::resolve_input_format() const {
  if (pad *peer = sink_pad_.peer(); peer != nullptr) {
    return video_format_from_caps(peer->current_caps());
  }
  return std::nullopt;
}

inline flow_return video_scale::chain(pad & /*sink_pad*/, buffer buf) {
  if (!input_format_) {
    input_format_ = resolve_input_format();
    if (!input_format_) {
      journal::warn("video_scale '{}' has no resolvable video/x-raw format on its sink pad's peer", name());
      return flow_return::error;
    }
  }
  const video_format &in_fmt = *input_format_;
  std::uint64_t out_w = output_width_.value_or(in_fmt.width);
  std::uint64_t out_h = output_height_.value_or(in_fmt.height);

  video_format out_fmt = in_fmt;
  out_fmt.width = out_w;
  out_fmt.height = out_h;
  src_pad_.set_caps(video_caps(out_fmt));

  if (out_w == in_fmt.width && out_h == in_fmt.height) {
    return src_pad_.push(std::move(buf));
  }

  auto src = buf.data();
  std::vector<std::byte> in_data(src.begin(), src.end());
  std::vector<std::byte> out_data;

  if (in_fmt.format == pixel_format::rgb24) {
    out_data = bilinear_scale_plane(in_data, in_fmt.width, in_fmt.height, out_w, out_h, 3);
  } else {
    // I420: scale each plane independently at its own resolution.
    std::uint64_t in_y_size = in_fmt.width * in_fmt.height;
    std::uint64_t in_cw = in_fmt.chroma_width(), in_ch = in_fmt.chroma_height();
    std::uint64_t in_c_size = in_cw * in_ch;

    std::vector<std::byte> y_plane(in_data.begin(), in_data.begin() + static_cast<std::ptrdiff_t>(in_y_size));
    std::vector<std::byte> u_plane(in_data.begin() + static_cast<std::ptrdiff_t>(in_y_size),
                                     in_data.begin() + static_cast<std::ptrdiff_t>(in_y_size + in_c_size));
    std::vector<std::byte> v_plane(in_data.begin() + static_cast<std::ptrdiff_t>(in_y_size + in_c_size),
                                     in_data.begin() + static_cast<std::ptrdiff_t>(in_y_size + 2 * in_c_size));

    std::uint64_t out_cw = out_fmt.chroma_width(), out_ch = out_fmt.chroma_height();
    auto y_out = bilinear_scale_plane(y_plane, in_fmt.width, in_fmt.height, out_w, out_h, 1);
    auto u_out = bilinear_scale_plane(u_plane, in_cw, in_ch, out_cw, out_ch, 1);
    auto v_out = bilinear_scale_plane(v_plane, in_cw, in_ch, out_cw, out_ch, 1);

    out_data.reserve(y_out.size() + u_out.size() + v_out.size());
    out_data.insert(out_data.end(), y_out.begin(), y_out.end());
    out_data.insert(out_data.end(), u_out.begin(), u_out.end());
    out_data.insert(out_data.end(), v_out.begin(), v_out.end());
  }

  buffer out(std::move(out_data));
  out.pts = buf.pts;
  out.dts = buf.dts;
  out.duration = buf.duration;
  out.offset = buf.offset;

  return src_pad_.push(std::move(out));
}

inline bool video_scale::handle_event(pad & /*sink_pad*/, const event &ev) { return src_pad_.send_event(ev); }

} // namespace cxflow::elements
