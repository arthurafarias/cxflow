// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/elements/pcm_format.hpp>
#include <cxflow/logging/journal.hpp>

namespace cxflow::elements {

namespace detail {

inline double sinc(double x) {
  if (std::abs(x) < 1e-9) {
    return 1.0;
  }
  double px = std::numbers::pi * x;
  return std::sin(px) / px;
}

// A Lanczos kernel (sinc(x)*sinc(x/a), zero outside |x|<a) - a standard,
// literally-windowed sinc, not a hand-rolled approximation. a=3 (a 6-input-
// sample-wide window) is a common, reasonable default that stays cheap
// per output sample.
constexpr int lanczos_a = 3;

inline double lanczos_kernel(double x) {
  if (std::abs(x) >= lanczos_a) {
    return 0.0;
  }
  return sinc(x) * sinc(x / lanczos_a);
}

} // namespace detail

// SRS-004 §5.2: sample-rate conversion via windowed-sinc (Lanczos)
// interpolation. The input format is auto-resolved from the sink pad's
// linked peer's caps; the output rate is explicit (set_output_rate()) -
// same "no downstream negotiation to read from" reasoning as
// audio_convert.hpp's output format, defaulting to a no-op (input rate
// unchanged) when unset.
//
// Deliberate simplification: each incoming buffer is resampled
// independently, clamping kernel taps that would reach past the buffer's
// own edges to its first/last sample, rather than carrying continuous
// filter state (a history tail) across buffer boundaries. This introduces
// a small discontinuity right at each buffer boundary but avoids the
// extra statefulness a true streaming resampler needs - acceptable for
// this pass; a stateful version is follow-on work if the boundary
// artifact ever matters for a real use case.
class audio_resample : public element {
public:
  explicit audio_resample(std::string name);

  void set_output_rate(std::uint64_t rate) { output_rate_ = rate; }

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  std::optional<pcm_format> resolve_input_format() const;

  pad &sink_pad_;
  pad &src_pad_;
  std::optional<pcm_format> input_format_;
  std::optional<std::uint64_t> output_rate_;
};

inline audio_resample::audio_resample(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline void audio_resample::register_type() {
  element_factory::register_type(
      "audio_resample", [](std::string name) { return std::make_shared<audio_resample>(std::move(name)); });
}

inline std::optional<pcm_format> audio_resample::resolve_input_format() const {
  if (pad *peer = sink_pad_.peer(); peer != nullptr) {
    return pcm_format_from_caps(peer->current_caps());
  }
  return std::nullopt;
}

inline flow_return audio_resample::chain(pad & /*sink_pad*/, buffer buf) {
  if (!input_format_) {
    input_format_ = resolve_input_format();
    if (!input_format_) {
      journal::warn("audio_resample '{}' has no resolvable audio/x-raw format on its sink pad's peer", name());
      return flow_return::error;
    }
  }
  const pcm_format &in_fmt = *input_format_;
  std::uint64_t out_rate = output_rate_.value_or(in_fmt.rate);

  pcm_format out_fmt = in_fmt;
  out_fmt.rate = out_rate;
  src_pad_.set_caps(pcm_caps(out_fmt));

  if (out_rate == in_fmt.rate || in_fmt.rate == 0 || in_fmt.channels == 0) {
    return src_pad_.push(std::move(buf));
  }

  auto in_data = buf.data();
  std::uint64_t in_frame_bytes = in_fmt.bytes_per_frame();
  std::uint64_t n_in = in_frame_bytes > 0 ? in_data.size() / in_frame_bytes : 0;
  if (n_in == 0) {
    return src_pad_.push(std::move(buf));
  }

  std::uint64_t n_out = static_cast<std::uint64_t>(
      std::llround(static_cast<double>(n_in) * static_cast<double>(out_rate) / static_cast<double>(in_fmt.rate)));
  double ratio = static_cast<double>(in_fmt.rate) / static_cast<double>(out_rate);

  auto sample_at = [&](std::int64_t idx, std::uint64_t c) -> double {
    std::int64_t clamped = std::clamp<std::int64_t>(idx, 0, static_cast<std::int64_t>(n_in) - 1);
    const std::byte *p = in_data.data() +
                          (static_cast<std::uint64_t>(clamped) * in_fmt.channels + c) * in_fmt.bytes_per_sample();
    return read_pcm_sample(p, in_fmt);
  };

  std::vector<std::byte> out_data(n_out * out_fmt.bytes_per_frame());
  for (std::uint64_t o = 0; o < n_out; ++o) {
    double pos = static_cast<double>(o) * ratio;
    auto center = static_cast<std::int64_t>(std::floor(pos));
    double frac = pos - static_cast<double>(center);

    for (std::uint64_t c = 0; c < in_fmt.channels; ++c) {
      double sum = 0.0;
      for (std::int64_t k = -detail::lanczos_a + 1; k <= detail::lanczos_a; ++k) {
        double weight = detail::lanczos_kernel(frac - static_cast<double>(k));
        if (weight == 0.0) {
          continue;
        }
        sum += sample_at(center + k, c) * weight;
      }
      write_pcm_sample(out_data.data() + (o * out_fmt.channels + c) * out_fmt.bytes_per_sample(), out_fmt, sum);
    }
  }

  buffer out(std::move(out_data));
  out.pts = buf.pts;
  out.dts = buf.dts;
  out.offset = buf.offset;
  if (out_fmt.rate > 0) {
    out.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(static_cast<double>(n_out) / static_cast<double>(out_fmt.rate)));
  }

  return src_pad_.push(std::move(out));
}

inline bool audio_resample::handle_event(pad & /*sink_pad*/, const event &ev) { return src_pad_.send_event(ev); }

} // namespace cxflow::elements
