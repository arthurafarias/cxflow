// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/pcm_format.hpp>
#include <cxflow/elements/testing/wav_demux_test.hpp> // reuses wav_demux_test_reference_bytes()
#include <cxflow/elements/wav_mux.hpp>
#include <cxflow/testing/test_group.hpp>

#include <cstddef>
#include <vector>

namespace cxflow::testing {

struct wav_mux_test : public test_group {
  wav_mux_test() : test_group("wav_mux", {
    {"produces a RIFF/WAVE file matching the hand-built reference byte-for-byte", [](test_context &ctx) {
      elements::wav_mux mux("mux");
      elements::pcm_format fmt;
      fmt.rate = 8000;
      fmt.channels = 1;
      fmt.bits_per_sample = 16;
      fmt.is_signed = true;
      mux.set_format(fmt);

      std::vector<std::byte> pcm{std::byte{0x01}, std::byte{0x00}, std::byte{0x02}, std::byte{0x00},
                                   std::byte{0xFF}, std::byte{0xFF}, std::byte{0x00}, std::byte{0x80}};

      std::vector<std::byte> muxed;
      bool eos_seen = false;

      element consumer("consumer");
      pad &consumer_sink = consumer.add_pad(std::make_unique<pad>("sink", pad::direction::sink, consumer));
      consumer_sink.set_active(true);
      consumer_sink.set_chain_function([&](pad &, buffer buf) {
        auto data = buf.data();
        muxed.insert(muxed.end(), data.begin(), data.end());
        return flow_return::ok;
      });
      consumer_sink.set_event_function([&](pad &, const event &ev) {
        eos_seen = ev.type == event_type::eos;
        return true;
      });
      mux.get_static_pad("src")->link(consumer_sink);

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*mux.get_static_pad("sink"));
      mux.get_static_pad("sink")->set_active(true);

      upstream_src.push(buffer(pcm));
      upstream_src.send_event(event{event_type::eos});

      ctx.check(eos_seen, "wav_mux should forward eos after emitting the file");
      ctx.check(muxed == wav_demux_test_reference_bytes(),
                 "the muxed bytes should exactly match the hand-built RIFF/WAVE reference");
    }},
    {"round-trips through wav_demux to the original PCM bytes and format", [](test_context &ctx) {
      elements::wav_mux mux("mux");
      elements::pcm_format fmt;
      fmt.rate = 44100;
      fmt.channels = 2;
      fmt.bits_per_sample = 16;
      fmt.is_signed = true;
      mux.set_format(fmt);

      std::vector<std::byte> pcm{std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x40},
                                   std::byte{0x50}, std::byte{0x60}, std::byte{0x70}, std::byte{0x80}};

      elements::wav_demux demux("demux");
      pad *demuxed_pad = nullptr;
      demux.pad_added.connect([&](element &, pad &p) { demuxed_pad = &p; });

      element consumer("consumer");
      pad &consumer_sink = consumer.add_pad(std::make_unique<pad>("sink", pad::direction::sink, consumer));
      consumer_sink.set_active(true);
      std::vector<std::byte> round_tripped;
      consumer_sink.set_chain_function([&](pad &, buffer buf) {
        auto data = buf.data();
        round_tripped.insert(round_tripped.end(), data.begin(), data.end());
        return flow_return::ok;
      });
      demux.pad_added.connect([&](element &, pad &p) { p.link(consumer_sink); });

      ctx.require(mux.get_static_pad("src")->link(*demux.get_static_pad("sink")),
                  "linking wav_mux to wav_demux should succeed");
      demux.get_static_pad("sink")->set_active(true);

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*mux.get_static_pad("sink"));
      mux.get_static_pad("sink")->set_active(true);

      upstream_src.push(buffer(pcm));
      upstream_src.send_event(event{event_type::eos});

      ctx.require(demuxed_pad != nullptr, "the muxed file should parse back successfully");
      auto round_fmt = elements::pcm_format_from_caps(demuxed_pad->current_caps());
      ctx.require(round_fmt.has_value(), "the round-tripped pad should carry PCM caps");
      ctx.check_equal(round_fmt->rate, std::uint64_t{44100});
      ctx.check_equal(round_fmt->channels, std::uint64_t{2});
      ctx.check_equal(round_fmt->bits_per_sample, std::uint64_t{16});
      ctx.check(round_tripped == pcm, "the round-tripped PCM bytes should match the original");
    }},
  }) {}
};

inline static wav_mux_test wav_mux_test_instance;

} // namespace cxflow::testing
