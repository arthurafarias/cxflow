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
#include <string>
#include <vector>

#include <cxflow/core/element.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/event.hpp>
#include <cxflow/elements/pcm_format.hpp>

namespace cxflow::elements {

// SRS-004 §5.3: RIFF/WAVE framing (Microsoft, public spec) over raw PCM.
// Wraps incoming "audio/x-raw" buffers in a WAVE header on eos. The format
// is read from the sink pad's linked peer's caps at eos time (e.g.
// wav_demux's own dynamically-added src pad already declares them via
// pcm_caps()) - the natural, automatic path for any pipeline built by
// linking to a caps-declaring upstream, text-grammar-driven (SRS-003 §5.1,
// which has no syntax to call a C++-only setter) or otherwise.
// set_format() remains available as an explicit fallback for a
// hand-assembled pipeline whose upstream never declares "audio/x-raw"
// caps at all (e.g. a bare buffer-pushing test double) - used only when
// the peer's caps don't resolve to a valid pcm_format.
//
// Deliberate simplification: a WAV header's ChunkSize/Subchunk2Size fields
// need the total PCM byte count, which isn't known until the last buffer
// has arrived - and a push pipeline has no way to seek back into an
// already-pushed buffer to patch a placeholder header afterward (unlike a
// GStreamer sink that owns a seekable fd). wav_mux instead accumulates
// every incoming buffer and emits the complete file - header plus all
// PCM data - as one buffer once eos arrives. Correct for any pipeline
// shape this catalog builds (a demuxer's output size is already known
// before the first buffer, and Wave 1 has no streaming-mux consumer that
// needs bytes before eos); a true streaming mux with header patch-up is
// follow-on work if a use case needs it.
class wav_mux : public element {
public:
  explicit wav_mux(std::string name);

  void set_format(pcm_format fmt) { format_ = fmt; }

  static void register_type();

private:
  flow_return chain(pad &sink_pad, buffer buf);
  bool handle_event(pad &sink_pad, const event &ev);

  pcm_format resolved_format() const;
  std::vector<std::byte> build_wav_file(const pcm_format &fmt) const;

  pad &sink_pad_;
  pad &src_pad_;
  pcm_format format_;
  std::vector<std::byte> pcm_data_;
};

inline wav_mux::wav_mux(std::string name)
    : element(std::move(name)), sink_pad_(add_pad(std::make_unique<pad>("sink", pad::direction::sink, *this))),
      src_pad_(add_pad(std::make_unique<pad>("src", pad::direction::src, *this))) {
  sink_pad_.set_chain_function([this](pad &p, buffer buf) { return chain(p, std::move(buf)); });
  sink_pad_.set_event_function([this](pad &p, const event &ev) { return handle_event(p, ev); });
}

inline void wav_mux::register_type() {
  element_factory::register_type("wav_mux",
                                  [](std::string name) { return std::make_shared<wav_mux>(std::move(name)); });
}

inline flow_return wav_mux::chain(pad & /*sink_pad*/, buffer buf) {
  auto data = buf.data();
  pcm_data_.insert(pcm_data_.end(), data.begin(), data.end());
  return flow_return::ok;
}

inline pcm_format wav_mux::resolved_format() const {
  if (pad *peer = sink_pad_.peer(); peer != nullptr) {
    if (auto peer_fmt = pcm_format_from_caps(peer->current_caps())) {
      return *peer_fmt;
    }
  }
  return format_;
}

inline std::vector<std::byte> wav_mux::build_wav_file(const pcm_format &fmt) const {
  std::uint32_t byte_rate = static_cast<std::uint32_t>(fmt.rate * fmt.channels * fmt.bytes_per_sample());
  std::uint16_t block_align = static_cast<std::uint16_t>(fmt.channels * fmt.bytes_per_sample());
  std::uint32_t data_size = static_cast<std::uint32_t>(pcm_data_.size());

  std::vector<std::byte> out;
  out.reserve(44 + pcm_data_.size());

  auto append_tag = [&out](const char *tag) {
    for (int i = 0; i < 4; ++i) {
      out.push_back(static_cast<std::byte>(tag[i]));
    }
  };

  append_tag("RIFF");
  write_le32(out, 36 + data_size);
  append_tag("WAVE");

  append_tag("fmt ");
  write_le32(out, 16); // PCM fmt chunk is always 16 bytes
  write_le16(out, 1);  // AudioFormat: 1 = PCM
  write_le16(out, static_cast<std::uint16_t>(fmt.channels));
  write_le32(out, static_cast<std::uint32_t>(fmt.rate));
  write_le32(out, byte_rate);
  write_le16(out, block_align);
  write_le16(out, static_cast<std::uint16_t>(fmt.bits_per_sample));

  append_tag("data");
  write_le32(out, data_size);
  out.insert(out.end(), pcm_data_.begin(), pcm_data_.end());

  return out;
}

inline bool wav_mux::handle_event(pad & /*sink_pad*/, const event &ev) {
  if (ev.type == event_type::eos) {
    src_pad_.push(buffer(build_wav_file(resolved_format())));
  }
  return src_pad_.send_event(ev);
}

} // namespace cxflow::elements
