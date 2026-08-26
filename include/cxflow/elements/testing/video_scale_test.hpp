// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/video_format.hpp>
#include <cxflow/elements/video_scale.hpp>
#include <cxflow/testing/test_group.hpp>

#include <cstddef>
#include <vector>

namespace cxflow::testing {

struct video_scale_test : public test_group {
  video_scale_test() : test_group("video_scale", {
    {"scaling a solid color preserves the color at the new resolution", [](test_context &ctx) {
      elements::video_scale scale("scale");
      scale.set_output_size(8, 8);

      elements::video_format in_fmt;
      in_fmt.format = elements::pixel_format::rgb24;
      in_fmt.width = 4;
      in_fmt.height = 4;

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.set_caps(elements::video_caps(in_fmt));
      upstream_src.link(*scale.get_static_pad("sink"));
      scale.get_static_pad("sink")->set_active(true);

      std::vector<buffer> received;
      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      downstream_sink.set_active(true);
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        received.push_back(std::move(buf));
        return flow_return::ok;
      });
      scale.get_static_pad("src")->link(downstream_sink);

      std::vector<std::byte> rgb(4 * 4 * 3);
      for (std::size_t i = 0; i < rgb.size(); i += 3) {
        rgb[i] = std::byte{10};
        rgb[i + 1] = std::byte{20};
        rgb[i + 2] = std::byte{30};
      }
      upstream_src.push(buffer(rgb));

      ctx.require_equal(received.size(), std::size_t{1});
      auto out = received[0].data();
      ctx.require_equal(out.size(), std::size_t{8 * 8 * 3});
      for (std::size_t i = 0; i < out.size(); i += 3) {
        ctx.check(out[i] == std::byte{10} && out[i + 1] == std::byte{20} && out[i + 2] == std::byte{30},
                   "a solid color should stay uniform after upscaling");
      }

      auto declared = elements::video_format_from_caps(scale.get_static_pad("src")->current_caps());
      ctx.require(declared.has_value(), "the src pad should declare the scaled size");
      ctx.check_equal(declared->width, std::uint64_t{8});
      ctx.check_equal(declared->height, std::uint64_t{8});
    }},
    {"scales I420 by resizing each plane at its own resolution", [](test_context &ctx) {
      elements::video_scale scale("scale");
      scale.set_output_size(4, 4);

      elements::video_format in_fmt;
      in_fmt.format = elements::pixel_format::i420;
      in_fmt.width = 8;
      in_fmt.height = 8;

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.set_caps(elements::video_caps(in_fmt));
      upstream_src.link(*scale.get_static_pad("sink"));
      scale.get_static_pad("sink")->set_active(true);

      std::vector<buffer> received;
      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      downstream_sink.set_active(true);
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        received.push_back(std::move(buf));
        return flow_return::ok;
      });
      scale.get_static_pad("src")->link(downstream_sink);

      std::vector<std::byte> i420(in_fmt.frame_size(), std::byte{100});
      upstream_src.push(buffer(i420));

      ctx.require_equal(received.size(), std::size_t{1});
      elements::video_format out_fmt;
      out_fmt.format = elements::pixel_format::i420;
      out_fmt.width = 4;
      out_fmt.height = 4;
      ctx.check_equal(received[0].data().size(), out_fmt.frame_size());
    }},
    {"with no explicit output size, passes the frame through unchanged", [](test_context &ctx) {
      elements::video_scale scale("scale");

      elements::video_format in_fmt;
      in_fmt.format = elements::pixel_format::rgb24;
      in_fmt.width = 4;
      in_fmt.height = 4;

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.set_caps(elements::video_caps(in_fmt));
      upstream_src.link(*scale.get_static_pad("sink"));
      scale.get_static_pad("sink")->set_active(true);

      std::vector<buffer> received;
      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      downstream_sink.set_active(true);
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        received.push_back(std::move(buf));
        return flow_return::ok;
      });
      scale.get_static_pad("src")->link(downstream_sink);

      std::vector<std::byte> rgb(4 * 4 * 3, std::byte{7});
      upstream_src.push(buffer(rgb));

      ctx.require_equal(received.size(), std::size_t{1});
      ctx.check(received[0].data().size() == rgb.size(), "no output size configured should leave the frame unchanged");
    }},
  }) {}
};

inline static video_scale_test video_scale_test_instance;

} // namespace cxflow::testing
