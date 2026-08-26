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

// AU's "encoding" field, restricted to the linear-PCM codes this catalog
// entry supports (§5.3's own framing: "au_mux/au_demux ... even simpler
// than WAV" scopes this to trivial framing, not the full encoding table -
// mu-law (1) and float (6/7) are out of scope here, same restriction
// au_demux applies).
inline std::optional<std::uint32_t> au_encoding_for_bits(std::uint64_t bits_per_sample) {
  switch (bits_per_sample) {
  case 8:
    return 2u;
  case 16:
    return 3u;
  case 24:
    return 4u;
  case 32:
    return 5u;
  default:
    return std::nullopt;
  }
}

} // namespace detail

// SRS-004 §5.3: Sun/NeXT .au framing (public spec) over linear PCM,
// big-endian on disk. Same "accumulate then emit one file at eos"
// simplification as wav_mux, for the same reason (no seekable-sink header
// patch-up in a push pipeline) - see wav_mux.hpp's class comment. Format
// resolution (peer caps first, set_format() as fallback) also mirrors
// wav_mux.hpp exactly - see its class comment.
class au_mux : public element {
public:
  explicit au_mux(std::string name);

  void set_format(pcm_format fmt) { format_ = fmt; }

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);
  pcm_format resolved_format() const;

  pad &sink_pad_;
  pad &src_pad_;
  pcm_format format_;
  std::vector<std::byte> pcm_data_; // accumulated in this codebase's internal little-endian convention
};

inline au_mux::au_mux(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline void au_mux::register_type() {
  element_factory::register_type("au_mux",
                                  [](std::string name) { return std::make_shared<au_mux>(std::move(name)); });
}

inline flow_return au_mux::chain(pad & /*sink_pad*/, buffer buf) {
  auto data = buf.data();
  pcm_data_.insert(pcm_data_.end(), data.begin(), data.end());
  return flow_return::ok;
}

inline pcm_format au_mux::resolved_format() const {
  if (pad *peer = sink_pad_.peer(); peer != nullptr) {
    if (auto peer_fmt = pcm_format_from_caps(peer->current_caps())) {
      return *peer_fmt;
    }
  }
  return format_;
}

inline bool au_mux::handle_event(pad & /*sink_pad*/, const event &ev) {
  if (ev.type == event_type::eos) {
    pcm_format fmt = resolved_format();
    auto encoding = detail::au_encoding_for_bits(fmt.bits_per_sample);
    if (!encoding.has_value()) {
      journal::warn("au_mux '{}' cannot mux {}-bit PCM: only 8/16/24/32-bit linear PCM is supported", name(),
                    fmt.bits_per_sample);
      message msg;
      msg.type = message_type::error;
      msg.source = weak_from_this();
      msg.debug_info = "au_mux '" + name() + "': unsupported bits-per-sample";
      post_message(std::move(msg));
    } else {
      std::vector<std::byte> out;
      out.reserve(24 + pcm_data_.size());
      for (char c : {'.', 's', 'n', 'd'}) {
        out.push_back(static_cast<std::byte>(c));
      }
      write_be32(out, 24); // data_offset: no annotation field
      write_be32(out, static_cast<std::uint32_t>(pcm_data_.size()));
      write_be32(out, *encoding);
      write_be32(out, static_cast<std::uint32_t>(fmt.rate));
      write_be32(out, static_cast<std::uint32_t>(fmt.channels));

      std::vector<std::byte> samples = pcm_data_;
      swap_sample_endianness(samples, fmt.bytes_per_sample());
      out.insert(out.end(), samples.begin(), samples.end());

      src_pad_.push(buffer(std::move(out)));
    }
  }
  return src_pad_.send_event(ev);
}

} // namespace cxflow::elements
