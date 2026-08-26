// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/video_format.hpp>
#include <cxflow/elements/video_rate.hpp>
#include <cxflow/testing/test_group.hpp>

#include <chrono>
#include <cstddef>
#include <vector>

namespace cxflow::testing {

namespace video_rate_test_detail {

inline buffer make_frame(char marker, buffer::clock_time pts) {
  buffer buf(std::vector<std::byte>{std::byte{static_cast<unsigned char>(marker)}});
  buf.pts = pts;
  return buf;
}

struct fixture {
  elements::video_rate rate_el{"rate"};
  element upstream{"upstream"};
  element downstream{"downstream"};
  pad *upstream_src;
  std::vector<buffer> received;

  explicit fixture(std::uint64_t out_num, std::uint64_t out_den) {
    rate_el.set_output_framerate(out_num, out_den);

    elements::video_format fmt;
    fmt.format = elements::pixel_format::rgb24;
    fmt.width = 4;
    fmt.height = 4;
    fmt.framerate_num = 30;
    fmt.framerate_den = 1;

    upstream_src = &upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
    upstream_src->set_caps(elements::video_caps(fmt));
    upstream_src->link(*rate_el.get_static_pad("sink"));
    rate_el.get_static_pad("sink")->set_active(true);

    pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
    downstream_sink.set_active(true);
    downstream_sink.set_chain_function([this](pad &, buffer buf) {
      received.push_back(std::move(buf));
      return flow_return::ok;
    });
    rate_el.get_static_pad("src")->link(downstream_sink);
  }

  char marker_of(std::size_t i) const { return static_cast<char>(std::to_integer<unsigned char>(received[i].data()[0])); }
};

} // namespace video_rate_test_detail

struct video_rate_test : public test_group {
  video_rate_test() : test_group("video_rate", {
    {"duplicates the most recent frame to fill a gap when input arrives slower than the output rate",
     [](test_context &ctx) {
       video_rate_test_detail::fixture f(2, 1); // 2fps output, 0.5s period

       f.upstream_src->push(video_rate_test_detail::make_frame('A', std::chrono::nanoseconds(0)));
       f.upstream_src->push(video_rate_test_detail::make_frame('B', std::chrono::nanoseconds(1'000'000'000)));

       ctx.require_equal(f.received.size(), std::size_t{3});
       ctx.check_equal(f.marker_of(0), 'A');
       ctx.check_equal(f.marker_of(1), 'A'); // duplicated to fill the 0.5s slot
       ctx.check_equal(f.marker_of(2), 'B');
       ctx.check(f.received[1].pts == std::chrono::nanoseconds(500'000'000), "the duplicate should land on its own slot's pts");
     }},
    {"drops frames superseded before their slot when input arrives faster than the output rate",
     [](test_context &ctx) {
       video_rate_test_detail::fixture f(1, 1); // 1fps output, 1s period

       f.upstream_src->push(video_rate_test_detail::make_frame('A', std::chrono::nanoseconds(0)));
       f.upstream_src->push(video_rate_test_detail::make_frame('B', std::chrono::nanoseconds(300'000'000)));
       f.upstream_src->push(video_rate_test_detail::make_frame('C', std::chrono::nanoseconds(600'000'000)));
       f.upstream_src->push(video_rate_test_detail::make_frame('D', std::chrono::nanoseconds(1'000'000'000)));

       ctx.require_equal(f.received.size(), std::size_t{2});
       ctx.check_equal(f.marker_of(0), 'A');
       ctx.check_equal(f.marker_of(1), 'C'); // B and D dropped
     }},
    {"declares the new framerate in its src pad's caps", [](test_context &ctx) {
      video_rate_test_detail::fixture f(24, 1);
      f.upstream_src->push(video_rate_test_detail::make_frame('A', std::chrono::nanoseconds(0)));

      auto declared = elements::video_format_from_caps(f.rate_el.get_static_pad("src")->current_caps());
      ctx.require(declared.has_value(), "the src pad should declare the new framerate");
      ctx.check_equal(declared->framerate_num, std::uint64_t{24});
    }},
  }) {}
};

inline static video_rate_test video_rate_test_instance;

} // namespace cxflow::testing
