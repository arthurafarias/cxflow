// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/core/message.hpp>
#include <cxflow/core/pipeline.hpp>
#include <cxflow/elements/pcm_format.hpp>
#include <cxflow/elements/wav_demux.hpp>
#include <cxflow/testing/test_group.hpp>

#include <chrono>
#include <cstddef>
#include <vector>

namespace cxflow::testing {

// A hand-built RIFF/WAVE file (Microsoft's public spec), independent of
// wav_mux's own encoder - NFR-2's "not only round-tripping through your
// own encoder" - 1 channel, 8000 Hz, 16-bit PCM, 4 samples: 1, 2, -1,
// -32768.
inline std::vector<std::byte> wav_demux_test_reference_bytes() {
  auto b = [](int v) { return std::byte{static_cast<unsigned char>(v)}; };
  return {
      // "RIFF"
      b('R'), b('I'), b('F'), b('F'),
      // ChunkSize = 36 + 8 = 44 (LE32)
      b(44), b(0), b(0), b(0),
      // "WAVE"
      b('W'), b('A'), b('V'), b('E'),
      // "fmt "
      b('f'), b('m'), b('t'), b(' '),
      // Subchunk1Size = 16 (LE32)
      b(16), b(0), b(0), b(0),
      // AudioFormat = 1 (PCM, LE16)
      b(1), b(0),
      // NumChannels = 1 (LE16)
      b(1), b(0),
      // SampleRate = 8000 = 0x00001F40 (LE32)
      b(0x40), b(0x1F), b(0), b(0),
      // ByteRate = 16000 = 0x00003E80 (LE32)
      b(0x80), b(0x3E), b(0), b(0),
      // BlockAlign = 2 (LE16)
      b(2), b(0),
      // BitsPerSample = 16 (LE16)
      b(16), b(0),
      // "data"
      b('d'), b('a'), b('t'), b('a'),
      // Subchunk2Size = 8 (LE32)
      b(8), b(0), b(0), b(0),
      // samples: 1, 2, -1, -32768 (LE16 each)
      b(0x01), b(0x00), b(0x02), b(0x00), b(0xFF), b(0xFF), b(0x00), b(0x80),
  };
}

struct wav_demux_test : public test_group {
  wav_demux_test() : test_group("wav_demux", {
    {"parses a RIFF/WAVE file into a dynamically-added src pad with the right caps", [](test_context &ctx) {
      elements::wav_demux demux("demux");

      pad *added_pad = nullptr;
      demux.pad_added.connect([&](element &, pad &p) { added_pad = &p; });

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*demux.get_static_pad("sink"));
      demux.get_static_pad("sink")->set_active(true);

      upstream_src.push(buffer(wav_demux_test_reference_bytes()));
      upstream_src.send_event(event{event_type::eos});

      ctx.require(added_pad != nullptr, "a successful parse should dynamically add a src pad (OPEN-M2)");
      ctx.check_equal(added_pad->name(), std::string("src"));

      auto fmt = elements::pcm_format_from_caps(added_pad->current_caps());
      ctx.require(fmt.has_value(), "the added pad's caps should describe the parsed PCM format");
      ctx.check_equal(fmt->rate, std::uint64_t{8000});
      ctx.check_equal(fmt->channels, std::uint64_t{1});
      ctx.check_equal(fmt->bits_per_sample, std::uint64_t{16});
      ctx.check(fmt->is_signed, "16-bit WAV PCM is signed");
    }},
    {"the parsed pad delivers the exact PCM payload bytes and then eos", [](test_context &ctx) {
      elements::wav_demux demux("demux");

      element consumer("consumer");
      pad &consumer_sink = consumer.add_pad(std::make_unique<pad>("sink", pad::direction::sink, consumer));
      consumer_sink.set_active(true);

      std::vector<std::byte> received_bytes;
      bool eos_seen = false;
      consumer_sink.set_chain_function([&](pad &, buffer buf) {
        auto data = buf.data();
        received_bytes.insert(received_bytes.end(), data.begin(), data.end());
        return flow_return::ok;
      });
      consumer_sink.set_event_function([&](pad &, const event &ev) {
        eos_seen = ev.type == event_type::eos;
        return true;
      });
      demux.pad_added.connect([&](element &, pad &p) { p.link(consumer_sink); });

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*demux.get_static_pad("sink"));
      demux.get_static_pad("sink")->set_active(true);

      upstream_src.push(buffer(wav_demux_test_reference_bytes()));
      upstream_src.send_event(event{event_type::eos});

      std::vector<std::byte> expected_pcm{std::byte{0x01}, std::byte{0x00}, std::byte{0x02}, std::byte{0x00},
                                            std::byte{0xFF}, std::byte{0xFF}, std::byte{0x00}, std::byte{0x80}};
      ctx.check(received_bytes == expected_pcm, "the demuxed PCM bytes should exactly match the 'data' chunk");
      ctx.check(eos_seen, "the demuxed pad should send eos after its one buffer");
    }},
    {"a non-RIFF file posts a bus error instead of crashing", [](test_context &ctx) {
      auto src = std::make_shared<elements::wav_demux>("demux");
      pipeline pipe("pipe");
      pipe.add(src);

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*src->get_static_pad("sink"));
      src->get_static_pad("sink")->set_active(true);

      std::vector<std::byte> garbage{std::byte{'n'}, std::byte{'o'}, std::byte{'p'}, std::byte{'e'}};
      upstream_src.push(buffer(std::move(garbage)));
      upstream_src.send_event(event{event_type::eos});

      auto msg = pipe.bus().pop(std::chrono::milliseconds(0));
      ctx.require(msg.has_value(), "a malformed file should post a bus message");
      ctx.check(msg->type == message_type::error, "the posted message should be an error");
    }},
  }) {}
};

inline static wav_demux_test wav_demux_test_instance;

} // namespace cxflow::testing
