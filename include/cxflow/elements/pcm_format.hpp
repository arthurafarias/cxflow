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
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <cxflow/core/caps.hpp>
#include <cxflow/core/structure.hpp>

namespace cxflow::elements {

// SRS-004 §5.3: a shared "audio/x-raw" caps convention for linear PCM,
// needed by every container in this catalog that carries raw audio
// (wav_mux/demux, au_mux/demux, and later the audio_* conversion entries) -
// not specified by the SRS itself (its catalog table is one-line "Notes"
// per entry, not a caps schema), so this is a from-scratch design decision,
// documented here rather than duplicated per container. Deliberately
// narrow: linear PCM only (no float, no companded encodings like u-law) -
// sufficient for every Wave 1 container, which are all PCM-only formats.
//
// Byte-order convention: every "audio/x-raw" buffer flowing between
// elements holds its samples little-endian, regardless of source
// container - WAV is natively little-endian, so wav_demux/wav_mux read/
// write it as-is; AU is natively big-endian, so au_demux/au_mux swap at
// their own boundary (swap_sample_endianness() below). This keeps every
// element downstream of a demuxer (audio_convert, volume, wav_mux, ...)
// working against one convention instead of needing to track per-source
// endianness.
struct pcm_format {
  std::uint64_t rate = 0;
  std::uint64_t channels = 0;
  std::uint64_t bits_per_sample = 0; // 8/16/24/32
  bool is_signed = true;             // false only for 8-bit WAV, which is conventionally unsigned

  std::uint64_t bytes_per_sample() const { return bits_per_sample / 8; }
  std::uint64_t bytes_per_frame() const { return bytes_per_sample() * channels; }
};

inline caps pcm_caps(const pcm_format &fmt) {
  structure s("audio/x-raw");
  s.set("rate", fmt.rate);
  s.set("channels", fmt.channels);
  s.set("bits-per-sample", fmt.bits_per_sample);
  s.set("signed", fmt.is_signed);

  caps c;
  c.add(std::move(s));
  return c;
}

inline std::optional<pcm_format> pcm_format_from_caps(const caps &c) {
  if (c.is_any() || c.structures().empty()) {
    return std::nullopt;
  }
  const structure &s = c.structures().front();
  if (s.name() != "audio/x-raw") {
    return std::nullopt;
  }

  auto rate = s.get("rate");
  auto channels = s.get("channels");
  auto bits = s.get("bits-per-sample");
  auto is_signed = s.get("signed");
  if (!rate || !channels || !bits || !is_signed) {
    return std::nullopt;
  }

  pcm_format fmt;
  fmt.rate = std::get<std::uint64_t>(*rate);
  fmt.channels = std::get<std::uint64_t>(*channels);
  fmt.bits_per_sample = std::get<std::uint64_t>(*bits);
  fmt.is_signed = std::get<bool>(*is_signed);
  return fmt;
}

// Reverses each bytes_per_sample-wide group in place - used by au_mux/
// au_demux to convert between AU's on-disk big-endian samples and this
// codebase's internal little-endian convention (above). A no-op for 8-bit
// samples (bytes_per_sample == 1), where there is nothing to reverse.
inline void swap_sample_endianness(std::span<std::byte> data, std::uint64_t bytes_per_sample) {
  if (bytes_per_sample <= 1) {
    return;
  }
  for (std::size_t i = 0; i + bytes_per_sample <= data.size(); i += bytes_per_sample) {
    for (std::uint64_t j = 0; j < bytes_per_sample / 2; ++j) {
      std::swap(data[i + j], data[i + bytes_per_sample - 1 - j]);
    }
  }
}

inline void write_le16(std::vector<std::byte> &out, std::uint16_t v) {
  out.push_back(static_cast<std::byte>(v & 0xFF));
  out.push_back(static_cast<std::byte>((v >> 8) & 0xFF));
}

inline void write_le32(std::vector<std::byte> &out, std::uint32_t v) {
  out.push_back(static_cast<std::byte>(v & 0xFF));
  out.push_back(static_cast<std::byte>((v >> 8) & 0xFF));
  out.push_back(static_cast<std::byte>((v >> 16) & 0xFF));
  out.push_back(static_cast<std::byte>((v >> 24) & 0xFF));
}

inline void write_be32(std::vector<std::byte> &out, std::uint32_t v) {
  out.push_back(static_cast<std::byte>((v >> 24) & 0xFF));
  out.push_back(static_cast<std::byte>((v >> 16) & 0xFF));
  out.push_back(static_cast<std::byte>((v >> 8) & 0xFF));
  out.push_back(static_cast<std::byte>(v & 0xFF));
}

inline std::uint16_t read_le16(const std::byte *p) {
  return static_cast<std::uint16_t>(std::to_integer<unsigned>(p[0]) | (std::to_integer<unsigned>(p[1]) << 8));
}

inline std::uint32_t read_le32(const std::byte *p) {
  return static_cast<std::uint32_t>(std::to_integer<unsigned>(p[0])) |
         (static_cast<std::uint32_t>(std::to_integer<unsigned>(p[1])) << 8) |
         (static_cast<std::uint32_t>(std::to_integer<unsigned>(p[2])) << 16) |
         (static_cast<std::uint32_t>(std::to_integer<unsigned>(p[3])) << 24);
}

inline std::uint32_t read_be32(const std::byte *p) {
  return (static_cast<std::uint32_t>(std::to_integer<unsigned>(p[0])) << 24) |
         (static_cast<std::uint32_t>(std::to_integer<unsigned>(p[1])) << 16) |
         (static_cast<std::uint32_t>(std::to_integer<unsigned>(p[2])) << 8) |
         static_cast<std::uint32_t>(std::to_integer<unsigned>(p[3]));
}

} // namespace cxflow::elements
