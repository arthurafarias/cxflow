// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <cxflow/core/caps.hpp>
#include <cxflow/core/structure.hpp>

namespace cxflow::elements {

// SRS-004 §5.2/§5.7: a shared "video/x-raw" caps convention, the video
// counterpart of pcm_format.hpp's "audio/x-raw" - not specified by the
// SRS's own one-line catalog notes, so a from-scratch decision. Narrow by
// design: two pixel formats are enough to exercise every Wave 1 video
// entry (video_test_src/video_convert/video_scale/video_rate) without
// needing a full fourcc registry - RGB24 (8-bit interleaved RGB, 3
// bytes/pixel) and I420 (planar 4:2:0 YUV, the format the vast majority of
// real container/codec payloads this project will eventually add actually
// use).
enum class pixel_format { rgb24, i420 };

inline const char *to_string(pixel_format f) {
  switch (f) {
  case pixel_format::rgb24:
    return "RGB24";
  case pixel_format::i420:
    return "I420";
  }
  return "unknown";
}

inline std::optional<pixel_format> pixel_format_from_string(const std::string &s) {
  if (s == "RGB24") {
    return pixel_format::rgb24;
  }
  if (s == "I420") {
    return pixel_format::i420;
  }
  return std::nullopt;
}

struct video_format {
  pixel_format format = pixel_format::rgb24;
  std::uint64_t width = 0;
  std::uint64_t height = 0;
  std::uint64_t framerate_num = 25;
  std::uint64_t framerate_den = 1;

  // I420's chroma planes are subsampled 2x2 and rounded up on an odd
  // dimension (the universal convention for 4:2:0 formats) - not a
  // simplification specific to this codebase.
  std::uint64_t chroma_width() const { return (width + 1) / 2; }
  std::uint64_t chroma_height() const { return (height + 1) / 2; }

  std::uint64_t frame_size() const {
    switch (format) {
    case pixel_format::rgb24:
      return width * height * 3;
    case pixel_format::i420:
      return width * height + 2 * chroma_width() * chroma_height();
    }
    return 0;
  }
};

inline caps video_caps(const video_format &fmt) {
  structure s("video/x-raw");
  s.set("format", std::string(to_string(fmt.format)));
  s.set("width", fmt.width);
  s.set("height", fmt.height);
  s.set("framerate-num", fmt.framerate_num);
  s.set("framerate-den", fmt.framerate_den);

  caps c;
  c.add(std::move(s));
  return c;
}

inline std::optional<video_format> video_format_from_caps(const caps &c) {
  if (c.is_any() || c.structures().empty()) {
    return std::nullopt;
  }
  const structure &s = c.structures().front();
  if (s.name() != "video/x-raw") {
    return std::nullopt;
  }

  auto format_str = s.get("format");
  auto width = s.get("width");
  auto height = s.get("height");
  auto fr_num = s.get("framerate-num");
  auto fr_den = s.get("framerate-den");
  if (!format_str || !width || !height || !fr_num || !fr_den) {
    return std::nullopt;
  }

  auto fmt_enum = pixel_format_from_string(std::get<std::string>(*format_str));
  if (!fmt_enum.has_value()) {
    return std::nullopt;
  }

  video_format fmt;
  fmt.format = *fmt_enum;
  fmt.width = std::get<std::uint64_t>(*width);
  fmt.height = std::get<std::uint64_t>(*height);
  fmt.framerate_num = std::get<std::uint64_t>(*fr_num);
  fmt.framerate_den = std::get<std::uint64_t>(*fr_den);
  return fmt;
}

namespace detail {

inline std::uint8_t clamp_to_byte(double v) { return static_cast<std::uint8_t>(std::clamp(v, 0.0, 255.0)); }

} // namespace detail

// ITU-R BT.601, full-range (0-255), the formulas most from-scratch/
// educational implementations use - not broadcast studio-swing (16-235).
// Documented here once since both directions and both RGB24<->I420
// conversion functions below share it.
inline void rgb_to_yuv(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t &y, std::uint8_t &u,
                        std::uint8_t &v) {
  double rd = r, gd = g, bd = b;
  y = detail::clamp_to_byte(0.299 * rd + 0.587 * gd + 0.114 * bd);
  u = detail::clamp_to_byte(-0.168736 * rd - 0.331264 * gd + 0.5 * bd + 128.0);
  v = detail::clamp_to_byte(0.5 * rd - 0.418688 * gd - 0.081312 * bd + 128.0);
}

inline void yuv_to_rgb(std::uint8_t y, std::uint8_t u, std::uint8_t v, std::uint8_t &r, std::uint8_t &g,
                        std::uint8_t &b) {
  double yd = y, ud = static_cast<double>(u) - 128.0, vd = static_cast<double>(v) - 128.0;
  r = detail::clamp_to_byte(yd + 1.402 * vd);
  g = detail::clamp_to_byte(yd - 0.344136 * ud - 0.714136 * vd);
  b = detail::clamp_to_byte(yd + 1.772 * ud);
}

// Chroma is averaged over each 2x2 luma block (a box filter - the simplest
// correct 4:4:4->4:2:0 downsampler, not the sharper polyphase filters real
// encoders use).
inline std::vector<std::byte> rgb24_to_i420(const std::vector<std::byte> &rgb, std::uint64_t width,
                                             std::uint64_t height) {
  video_format fmt;
  fmt.format = pixel_format::i420;
  fmt.width = width;
  fmt.height = height;
  std::vector<std::byte> out(fmt.frame_size());

  std::uint64_t cw = fmt.chroma_width();
  auto *y_plane = out.data();
  auto *u_plane = y_plane + width * height;
  auto *v_plane = u_plane + cw * fmt.chroma_height();

  std::vector<std::uint8_t> full_u(width * height);
  std::vector<std::uint8_t> full_v(width * height);

  for (std::uint64_t row = 0; row < height; ++row) {
    for (std::uint64_t col = 0; col < width; ++col) {
      std::uint64_t px = (row * width + col) * 3;
      std::uint8_t r = std::to_integer<std::uint8_t>(rgb[px]);
      std::uint8_t g = std::to_integer<std::uint8_t>(rgb[px + 1]);
      std::uint8_t b = std::to_integer<std::uint8_t>(rgb[px + 2]);
      std::uint8_t y, u, v;
      rgb_to_yuv(r, g, b, y, u, v);
      y_plane[row * width + col] = static_cast<std::byte>(y);
      full_u[row * width + col] = u;
      full_v[row * width + col] = v;
    }
  }

  for (std::uint64_t crow = 0; crow < fmt.chroma_height(); ++crow) {
    for (std::uint64_t ccol = 0; ccol < cw; ++ccol) {
      std::uint64_t r0 = crow * 2, c0 = ccol * 2;
      std::uint64_t count = 0;
      unsigned usum = 0, vsum = 0;
      for (std::uint64_t dr = 0; dr < 2; ++dr) {
        for (std::uint64_t dc = 0; dc < 2; ++dc) {
          std::uint64_t r = r0 + dr, c = c0 + dc;
          if (r < height && c < width) {
            usum += full_u[r * width + c];
            vsum += full_v[r * width + c];
            ++count;
          }
        }
      }
      u_plane[crow * cw + ccol] = static_cast<std::byte>(usum / count);
      v_plane[crow * cw + ccol] = static_cast<std::byte>(vsum / count);
    }
  }

  return out;
}

// Chroma is nearest-neighbor upsampled (every 2x2 luma block shares its
// source chroma sample) - the simplest correct 4:2:0->4:4:4 upsampler, not
// the smoother bilinear-chroma reconstruction real decoders use.
inline std::vector<std::byte> i420_to_rgb24(const std::vector<std::byte> &i420, std::uint64_t width,
                                             std::uint64_t height) {
  video_format fmt;
  fmt.format = pixel_format::i420;
  fmt.width = width;
  fmt.height = height;
  std::uint64_t cw = fmt.chroma_width();

  const auto *y_plane = i420.data();
  const auto *u_plane = y_plane + width * height;
  const auto *v_plane = u_plane + cw * fmt.chroma_height();

  std::vector<std::byte> out(width * height * 3);
  for (std::uint64_t row = 0; row < height; ++row) {
    for (std::uint64_t col = 0; col < width; ++col) {
      std::uint8_t y = std::to_integer<std::uint8_t>(y_plane[row * width + col]);
      std::uint8_t u = std::to_integer<std::uint8_t>(u_plane[(row / 2) * cw + (col / 2)]);
      std::uint8_t v = std::to_integer<std::uint8_t>(v_plane[(row / 2) * cw + (col / 2)]);
      std::uint8_t r, g, b;
      yuv_to_rgb(y, u, v, r, g, b);
      std::uint64_t px = (row * width + col) * 3;
      out[px] = static_cast<std::byte>(r);
      out[px + 1] = static_cast<std::byte>(g);
      out[px + 2] = static_cast<std::byte>(b);
    }
  }
  return out;
}

// Generic bilinear resample of one interleaved plane (channels=3 for
// RGB24; channels=1 for a single I420 plane, called once per plane by
// video_scale so the same routine handles both formats).
inline std::vector<std::byte> bilinear_scale_plane(const std::vector<std::byte> &src, std::uint64_t src_w,
                                                     std::uint64_t src_h, std::uint64_t dst_w, std::uint64_t dst_h,
                                                     std::uint64_t channels) {
  std::vector<std::byte> out(dst_w * dst_h * channels);
  if (src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0) {
    return out;
  }

  double x_scale = static_cast<double>(src_w) / static_cast<double>(dst_w);
  double y_scale = static_cast<double>(src_h) / static_cast<double>(dst_h);

  auto sample = [&](std::uint64_t x, std::uint64_t y, std::uint64_t ch) -> double {
    x = std::min(x, src_w - 1);
    y = std::min(y, src_h - 1);
    return static_cast<double>(std::to_integer<std::uint8_t>(src[(y * src_w + x) * channels + ch]));
  };

  for (std::uint64_t dy = 0; dy < dst_h; ++dy) {
    double sy = (static_cast<double>(dy) + 0.5) * y_scale - 0.5;
    std::uint64_t y0 = static_cast<std::uint64_t>(std::max(0.0, std::floor(sy)));
    double fy = std::clamp(sy - static_cast<double>(y0), 0.0, 1.0);

    for (std::uint64_t dx = 0; dx < dst_w; ++dx) {
      double sx = (static_cast<double>(dx) + 0.5) * x_scale - 0.5;
      std::uint64_t x0 = static_cast<std::uint64_t>(std::max(0.0, std::floor(sx)));
      double fx = std::clamp(sx - static_cast<double>(x0), 0.0, 1.0);

      for (std::uint64_t ch = 0; ch < channels; ++ch) {
        double top = sample(x0, y0, ch) * (1.0 - fx) + sample(x0 + 1, y0, ch) * fx;
        double bottom = sample(x0, y0 + 1, ch) * (1.0 - fx) + sample(x0 + 1, y0 + 1, ch) * fx;
        double value = top * (1.0 - fy) + bottom * fy;
        out[(dy * dst_w + dx) * channels + ch] = static_cast<std::byte>(detail::clamp_to_byte(value));
      }
    }
  }

  return out;
}

} // namespace cxflow::elements
