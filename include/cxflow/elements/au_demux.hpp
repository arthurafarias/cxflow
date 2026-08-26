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
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/core/message.hpp>
#include <cxflow/elements/pcm_format.hpp>
#include <cxflow/logging/journal.hpp>

namespace cxflow::elements {

namespace detail {

// The inverse of au_mux's au_encoding_for_bits() - see au_mux.hpp for why
// this is restricted to linear PCM only.
inline std::optional<std::uint64_t> au_bits_for_encoding(std::uint32_t encoding) {
  switch (encoding) {
  case 2:
    return std::uint64_t{8};
  case 3:
    return std::uint64_t{16};
  case 4:
    return std::uint64_t{24};
  case 5:
    return std::uint64_t{32};
  default:
    return std::nullopt;
  }
}

} // namespace detail

// SRS-004 §5.3/§8 OPEN-M2: parses a Sun/NeXT .au file into one raw PCM
// stream, on a dynamically-added "src" pad - same accumulate-then-parse-at-
// eos and pad-added shape as wav_demux.hpp (see its class comment).
class au_demux : public element {
public:
  explicit au_demux(std::string name);

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  void post_parse_error(const std::string &reason);

  pad &sink_pad_;
  std::vector<std::byte> accumulated_;
};

inline au_demux::au_demux(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline void au_demux::register_type() {
  element_factory::register_type("au_demux",
                                  [](std::string name) { return std::make_shared<au_demux>(std::move(name)); });
}

inline flow_return au_demux::chain(pad & /*sink_pad*/, buffer buf) {
  auto data = buf.data();
  accumulated_.insert(accumulated_.end(), data.begin(), data.end());
  return flow_return::ok;
}

inline void au_demux::post_parse_error(const std::string &reason) {
  journal::warn("au_demux '{}' failed to parse: {}", name(), reason);
  message msg;
  msg.type = message_type::error;
  msg.source = weak_from_this();
  msg.debug_info = "au_demux '" + name() + "': " + reason;
  post_message(std::move(msg));
}

inline bool au_demux::handle_event(pad & /*sink_pad*/, const event &ev) {
  if (ev.type != event_type::eos) {
    return true;
  }

  const auto &d = accumulated_;
  if (d.size() < 24 || std::memcmp(d.data(), ".snd", 4) != 0) {
    post_parse_error("not an AU (.snd) file");
    return true;
  }

  std::uint32_t data_offset = read_be32(d.data() + 4);
  std::uint32_t data_size = read_be32(d.data() + 8);
  std::uint32_t encoding = read_be32(d.data() + 12);
  std::uint32_t rate = read_be32(d.data() + 16);
  std::uint32_t channels = read_be32(d.data() + 20);

  auto bits = detail::au_bits_for_encoding(encoding);
  if (!bits.has_value()) {
    post_parse_error("unsupported AU encoding (only 8/16/24/32-bit linear PCM is supported)");
    return true;
  }
  if (data_offset > d.size()) {
    post_parse_error("data_offset points past the end of the file");
    return true;
  }

  // 0xFFFFFFFF conventionally means "unknown length" - the AU spec allows
  // it for streamed files; treat it as "everything after data_offset".
  std::size_t available = d.size() - data_offset;
  std::size_t size = (data_size == 0xFFFFFFFFu || data_size > available) ? available : data_size;

  pcm_format fmt;
  fmt.rate = rate;
  fmt.channels = channels;
  fmt.bits_per_sample = *bits;
  fmt.is_signed = true; // every linear encoding this entry supports is signed

  pad &src_pad = add_pad(std::make_unique<pad>("src", pad::direction::src, *this));
  src_pad.set_caps(pcm_caps(fmt));

  std::vector<std::byte> pcm(d.begin() + static_cast<std::ptrdiff_t>(data_offset),
                              d.begin() + static_cast<std::ptrdiff_t>(data_offset + size));
  swap_sample_endianness(pcm, fmt.bytes_per_sample()); // AU on-disk is big-endian; this codebase's convention is little-endian

  src_pad.push(buffer(std::move(pcm)));
  src_pad.send_event(event{event_type::eos});

  return true;
}

} // namespace cxflow::elements
