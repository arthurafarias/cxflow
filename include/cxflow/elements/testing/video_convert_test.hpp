// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/video_convert.hpp>
#include <cxflow/elements/video_format.hpp>
#include <cxflow/testing/test_group.hpp>

#include <cstddef>
#include <cstdlib>
#include <vector>

namespace cxflow::testing {

struct video_convert_test : public test_group {
  video_convert_test() : test_group("video_convert", {
    {"converts RGB24 to I420 with the correct frame size and declared caps", [](test_context &ctx) {
      elements::video_convert conv("conv");
      conv.set_output_format(elements::pixel_format::i420);

      elements::video_format in_fmt;
      in_fmt.format = elements::pixel_format::rgb24;
      in_fmt.width = 4;
      in_fmt.height = 4;

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.set_caps(elements::video_caps(in_fmt));
      upstream_src.link(*conv.get_static_pad("sink"));
      conv.get_static_pad("sink")->set_active(true);

      std::vector<buffer> received;
      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      downstream_sink.set_active(true);
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        received.push_back(std::move(buf));
        return flow_return::ok;
      });
      conv.get_static_pad("src")->link(downstream_sink);

      std::vector<std::byte> rgb(4 * 4 * 3, std::byte{128});
      upstream_src.push(buffer(rgb));

      ctx.require_equal(received.size(), std::size_t{1});
      ctx.check_equal(received[0].data().size(), std::size_t{4 * 4 + 2 * 2 * 2}); // Y + 2x(2x2 chroma)

      auto declared = elements::video_format_from_caps(conv.get_static_pad("src")->current_caps());
      ctx.require(declared.has_value(), "the src pad should declare the converted format");
      ctx.check(declared->format == elements::pixel_format::i420, "the declared format should be I420");
    }},
    {"round-trips a solid color through RGB24 -> I420 -> RGB24 within rounding tolerance", [](test_context &ctx) {
      elements::video_convert to_i420("to_i420");
      to_i420.set_output_format(elements::pixel_format::i420);
      elements::video_convert to_rgb("to_rgb");
      to_rgb.set_output_format(elements::pixel_format::rgb24);

      elements::video_format in_fmt;
      in_fmt.format = elements::pixel_format::rgb24;
      in_fmt.width = 8;
      in_fmt.height = 8;

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.set_caps(elements::video_caps(in_fmt));
      upstream_src.link(*to_i420.get_static_pad("sink"));
      to_i420.get_static_pad("sink")->set_active(true);

      ctx.require(to_i420.get_static_pad("src")->link(*to_rgb.get_static_pad("sink")),
                  "linking the two video_convert stages should succeed");
      to_rgb.get_static_pad("sink")->set_active(true);

      std::vector<buffer> received;
      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      downstream_sink.set_active(true);
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        received.push_back(std::move(buf));
        return flow_return::ok;
      });
      to_rgb.get_static_pad("src")->link(downstream_sink);

      // A solid, non-gray color so R/G/B asymmetry is actually exercised.
      std::vector<std::byte> rgb(8 * 8 * 3);
      for (std::size_t i = 0; i < rgb.size(); i += 3) {
        rgb[i] = std::byte{200};
        rgb[i + 1] = std::byte{100};
        rgb[i + 2] = std::byte{50};
      }
      upstream_src.push(buffer(rgb));

      ctx.require_equal(received.size(), std::size_t{1});
      auto out = received[0].data();
      ctx.require_equal(out.size(), rgb.size());
      for (std::size_t i = 0; i < out.size(); ++i) {
        int diff = std::abs(static_cast<int>(std::to_integer<unsigned char>(out[i])) -
                             static_cast<int>(std::to_integer<unsigned char>(rgb[i])));
        ctx.check(diff <= 3, "a solid color should round-trip through RGB24->I420->RGB24 within a few LSBs");
      }
    }},
  }) {}
};

inline static video_convert_test video_convert_test_instance;

} // namespace cxflow::testing
