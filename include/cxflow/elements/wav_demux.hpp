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

// SRS-004 §5.3/§8 OPEN-M2: parses a RIFF/WAVE file into one raw PCM stream,
// exposed as a dynamically-added "src" pad (OPEN-M2's pad-added pattern)
// once the format is known - there is no src pad until parsing succeeds,
// since a demuxer cannot declare caps before it has read the "fmt " chunk.
//
// Simplification symmetric with wav_mux: incoming bytes (which may arrive
// split across several buffers, e.g. from file_src's own "blocksize"
// chunking) are accumulated on the sink pad and parsed once as a whole on
// eos, rather than incrementally chunk-by-chunk as bytes trickle in.
// Linear PCM only (WAVE_FORMAT_PCM, AudioFormat == 1) - WAVE_FORMAT_EXTENSIBLE
// and any compressed payload are out of this entry's scope, matching the
// catalog's own "trivial framing over raw PCM" framing for this row.
class wav_demux : public element {
public:
  explicit wav_demux(std::string name);

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  void post_parse_error(const std::string &reason);

  pad &sink_pad_;
  std::vector<std::byte> accumulated_;
};

inline wav_demux::wav_demux(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline void wav_demux::register_type() {
  element_factory::register_type("wav_demux",
                                  [](std::string name) { return std::make_shared<wav_demux>(std::move(name)); });
}

inline flow_return wav_demux::chain(pad & /*sink_pad*/, buffer buf) {
  auto data = buf.data();
  accumulated_.insert(accumulated_.end(), data.begin(), data.end());
  return flow_return::ok;
}

inline void wav_demux::post_parse_error(const std::string &reason) {
  journal::warn("wav_demux '{}' failed to parse: {}", name(), reason);
  message msg;
  msg.type = message_type::error;
  msg.source = weak_from_this();
  msg.debug_info = "wav_demux '" + name() + "': " + reason;
  post_message(std::move(msg));
}

inline bool wav_demux::handle_event(pad & /*sink_pad*/, const event &ev) {
  if (ev.type != event_type::eos) {
    return true;
  }

  const auto &d = accumulated_;
  auto tag_at = [&](std::size_t offset, const char *tag) {
    return offset + 4 <= d.size() && std::memcmp(d.data() + offset, tag, 4) == 0;
  };

  if (d.size() < 12 || !tag_at(0, "RIFF") || !tag_at(8, "WAVE")) {
    post_parse_error("not a RIFF/WAVE file");
    return true;
  }

  std::optional<pcm_format> fmt;
  std::size_t data_offset = 0;
  std::size_t data_size = 0;
  bool found_data = false;

  std::size_t pos = 12;
  while (pos + 8 <= d.size()) {
    const char *chunk_id = reinterpret_cast<const char *>(d.data() + pos);
    std::uint32_t chunk_size = read_le32(d.data() + pos + 4);
    std::size_t payload_offset = pos + 8;

    if (payload_offset + chunk_size > d.size()) {
      break; // truncated chunk - stop, use whatever was already found
    }

    if (std::memcmp(chunk_id, "fmt ", 4) == 0 && chunk_size >= 16) {
      const std::byte *p = d.data() + payload_offset;
      std::uint16_t audio_format = read_le16(p);
      std::uint16_t channels = read_le16(p + 2);
      std::uint32_t rate = read_le32(p + 4);
      std::uint16_t bits_per_sample = read_le16(p + 14);

      if (audio_format != 1) {
        post_parse_error("unsupported WAVE AudioFormat (only linear PCM is supported)");
        return true;
      }

      pcm_format parsed;
      parsed.rate = rate;
      parsed.channels = channels;
      parsed.bits_per_sample = bits_per_sample;
      parsed.is_signed = bits_per_sample != 8; // WAV convention: 8-bit PCM is unsigned, everything wider is signed
      fmt = parsed;
    } else if (std::memcmp(chunk_id, "data", 4) == 0) {
      data_offset = payload_offset;
      data_size = chunk_size;
      found_data = true;
    }

    pos = payload_offset + chunk_size + (chunk_size % 2); // chunks are word-aligned
  }

  if (!fmt.has_value()) {
    post_parse_error("no 'fmt ' chunk found");
    return true;
  }
  if (!found_data) {
    post_parse_error("no 'data' chunk found");
    return true;
  }

  pad &src_pad = add_pad(std::make_unique<pad>("src", pad::direction::src, *this));
  src_pad.set_caps(pcm_caps(*fmt));

  std::vector<std::byte> pcm(d.begin() + static_cast<std::ptrdiff_t>(data_offset),
                              d.begin() + static_cast<std::ptrdiff_t>(data_offset + data_size));
  src_pad.push(buffer(std::move(pcm)));
  src_pad.send_event(event{event_type::eos});

  return true;
}

} // namespace cxflow::elements
