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
#include <cxflow/elements/au_demux.hpp>
#include <cxflow/elements/pcm_format.hpp>
#include <cxflow/testing/test_group.hpp>

#include <chrono>
#include <cstddef>
#include <vector>

namespace cxflow::testing {

// A hand-built Sun/NeXT .au file (public spec), independent of au_mux's own
// encoder - NFR-2's "not only round-tripping through your own encoder" -
// 1 channel, 8000 Hz, 16-bit linear PCM (encoding 3), the same 4 samples
// as wav_demux_test.hpp's reference: 1, 2, -1, -32768, stored big-endian
// on disk per the AU spec.
inline std::vector<std::byte> au_demux_test_reference_bytes() {
  auto b = [](int v) { return std::byte{static_cast<unsigned char>(v)}; };
  return {
      // ".snd"
      b('.'), b('s'), b('n'), b('d'),
      // data_offset = 24 (BE32)
      b(0), b(0), b(0), b(24),
      // data_size = 8 (BE32)
      b(0), b(0), b(0), b(8),
      // encoding = 3 (16-bit linear PCM, BE32)
      b(0), b(0), b(0), b(3),
      // sample_rate = 8000 = 0x00001F40 (BE32)
      b(0), b(0), b(0x1F), b(0x40),
      // channels = 1 (BE32)
      b(0), b(0), b(0), b(1),
      // samples: 1, 2, -1, -32768 (BE16 each)
      b(0x00), b(0x01), b(0x00), b(0x02), b(0xFF), b(0xFF), b(0x80), b(0x00),
  };
}

struct au_demux_test : public test_group {
  au_demux_test() : test_group("au_demux", {
    {"parses an AU file into a dynamically-added src pad with the right caps", [](test_context &ctx) {
      elements::au_demux demux("demux");

      pad *added_pad = nullptr;
      demux.pad_added.connect([&](element &, pad &p) { added_pad = &p; });

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*demux.get_static_pad("sink"));
      demux.get_static_pad("sink")->set_active(true);

      upstream_src.push(buffer(au_demux_test_reference_bytes()));
      upstream_src.send_event(event{event_type::eos});

      ctx.require(added_pad != nullptr, "a successful parse should dynamically add a src pad");
      auto fmt = elements::pcm_format_from_caps(added_pad->current_caps());
      ctx.require(fmt.has_value(), "the added pad's caps should describe the parsed PCM format");
      ctx.check_equal(fmt->rate, std::uint64_t{8000});
      ctx.check_equal(fmt->channels, std::uint64_t{1});
      ctx.check_equal(fmt->bits_per_sample, std::uint64_t{16});
    }},
    {"the parsed pad delivers little-endian PCM bytes, converted from AU's big-endian on-disk samples",
     [](test_context &ctx) {
       elements::au_demux demux("demux");

       element consumer("consumer");
       pad &consumer_sink = consumer.add_pad(std::make_unique<pad>("sink", pad::direction::sink, consumer));
       consumer_sink.set_active(true);

       std::vector<std::byte> received_bytes;
       consumer_sink.set_chain_function([&](pad &, buffer buf) {
         auto data = buf.data();
         received_bytes.insert(received_bytes.end(), data.begin(), data.end());
         return flow_return::ok;
       });
       demux.pad_added.connect([&](element &, pad &p) { p.link(consumer_sink); });

       element upstream("upstream");
       pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
       upstream_src.link(*demux.get_static_pad("sink"));
       demux.get_static_pad("sink")->set_active(true);

       upstream_src.push(buffer(au_demux_test_reference_bytes()));
       upstream_src.send_event(event{event_type::eos});

       std::vector<std::byte> expected_le_pcm{std::byte{0x01}, std::byte{0x00}, std::byte{0x02}, std::byte{0x00},
                                                std::byte{0xFF}, std::byte{0xFF}, std::byte{0x00}, std::byte{0x80}};
       ctx.check(received_bytes == expected_le_pcm,
                  "big-endian AU samples should be byte-swapped to this codebase's internal little-endian convention");
     }},
    {"a non-AU file posts a bus error instead of crashing", [](test_context &ctx) {
      auto src = std::make_shared<elements::au_demux>("demux");
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

inline static au_demux_test au_demux_test_instance;

} // namespace cxflow::testing
