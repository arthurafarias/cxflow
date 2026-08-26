// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/core/pipeline.hpp>
#include <cxflow/elements/au_mux.hpp>
#include <cxflow/elements/pcm_format.hpp>
#include <cxflow/elements/testing/au_demux_test.hpp> // reuses au_demux_test_reference_bytes()
#include <cxflow/testing/test_group.hpp>

#include <chrono>
#include <cstddef>
#include <vector>

namespace cxflow::testing {

struct au_mux_test : public test_group {
  au_mux_test() : test_group("au_mux", {
    {"produces an AU file matching the hand-built reference byte-for-byte", [](test_context &ctx) {
      elements::au_mux mux("mux");
      elements::pcm_format fmt;
      fmt.rate = 8000;
      fmt.channels = 1;
      fmt.bits_per_sample = 16;
      fmt.is_signed = true;
      mux.set_format(fmt);

      // This codebase's internal little-endian convention - the same 4
      // samples (1, 2, -1, -32768) as au_demux_test.hpp's reference,
      // au_mux is responsible for byte-swapping to AU's on-disk
      // big-endian.
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

      ctx.check(eos_seen, "au_mux should forward eos after emitting the file");
      ctx.check(muxed == au_demux_test_reference_bytes(),
                 "the muxed bytes should exactly match the hand-built AU reference");
    }},
    {"an unsupported bit depth posts a bus error instead of emitting a corrupt file", [](test_context &ctx) {
      auto mux = std::make_shared<elements::au_mux>("mux");
      elements::pcm_format fmt;
      fmt.rate = 8000;
      fmt.channels = 1;
      fmt.bits_per_sample = 12; // not one of AU's 8/16/24/32-bit linear encodings
      mux->set_format(fmt);

      pipeline pipe("pipe");
      pipe.add(mux);

      bool pushed_anything = false;
      element consumer("consumer");
      pad &consumer_sink = consumer.add_pad(std::make_unique<pad>("sink", pad::direction::sink, consumer));
      consumer_sink.set_active(true);
      consumer_sink.set_chain_function([&](pad &, buffer) {
        pushed_anything = true;
        return flow_return::ok;
      });
      mux->get_static_pad("src")->link(consumer_sink);

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*mux->get_static_pad("sink"));
      mux->get_static_pad("sink")->set_active(true);

      upstream_src.send_event(event{event_type::eos});

      ctx.check(!pushed_anything, "an unsupported bit depth should not emit a file");
      auto msg = pipe.bus().pop(std::chrono::milliseconds(0));
      ctx.require(msg.has_value(), "an unsupported bit depth should post a bus message");
      ctx.check(msg->type == message_type::error, "the posted message should be an error");
    }},
  }) {}
};

inline static au_mux_test au_mux_test_instance;

} // namespace cxflow::testing
