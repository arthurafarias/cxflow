// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/tee.hpp>
#include <cxflow/testing/test_group.hpp>

#include <cstddef>

namespace cxflow::testing {

struct tee_test : public test_group {
  tee_test() : test_group("tee", {
    {"fans one buffer out to every requested src pad", [](test_context &ctx) {
      elements::tee t("t");
      pad &sink_pad = *t.get_static_pad("sink");

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      ctx.require(upstream_src.link(sink_pad), "linking upstream to tee's sink pad should succeed");
      sink_pad.set_active(true);

      element branch_a("branch_a");
      pad &branch_a_sink = branch_a.add_pad(std::make_unique<pad>("sink", pad::direction::sink, branch_a));
      element branch_b("branch_b");
      pad &branch_b_sink = branch_b.add_pad(std::make_unique<pad>("sink", pad::direction::sink, branch_b));
      branch_a_sink.set_active(true);
      branch_b_sink.set_active(true);

      pad &tee_out_a = t.request_src_pad();
      pad &tee_out_b = t.request_src_pad();
      ctx.require(tee_out_a.link(branch_a_sink), "linking tee's first requested pad should succeed");
      ctx.require(tee_out_b.link(branch_b_sink), "linking tee's second requested pad should succeed");

      int received_a = 0;
      int received_b = 0;
      branch_a_sink.set_chain_function([&](pad &, buffer) {
        ++received_a;
        return flow_return::ok;
      });
      branch_b_sink.set_chain_function([&](pad &, buffer) {
        ++received_b;
        return flow_return::ok;
      });

      ctx.require(upstream_src.push(buffer()) == flow_return::ok, "pushing through tee should succeed");
      ctx.check_equal(received_a, 1);
      ctx.check_equal(received_b, 1);
    }},
    {"an unlinked tee reports not_linked", [](test_context &ctx) {
      elements::tee t("t");
      pad &sink_pad = *t.get_static_pad("sink");
      sink_pad.set_active(true);

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(sink_pad);

      ctx.check(upstream_src.push(buffer()) == flow_return::not_linked,
                 "a tee with no requested pads should report not_linked");
    }},
  }) {}
};

inline static tee_test tee_test_instance;

} // namespace cxflow::testing
