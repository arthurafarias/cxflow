// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/valve.hpp>
#include <cxflow/testing/test_group.hpp>

namespace cxflow::testing {

struct valve_test : public test_group {
  valve_test() : test_group("valve", {
    {"forwards buffers when open (the default)", [](test_context &ctx) {
      elements::valve v("v");
      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      ctx.require(v.get_static_pad("src")->link(downstream_sink), "linking valve's src pad should succeed");
      downstream_sink.set_active(true);

      int received = 0;
      downstream_sink.set_chain_function([&](pad &, buffer) {
        ++received;
        return flow_return::ok;
      });

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*v.get_static_pad("sink"));
      v.get_static_pad("sink")->set_active(true);

      ctx.check(upstream_src.push(buffer()) == flow_return::ok, "pushing through an open valve should succeed");
      ctx.check_equal(received, 1);
    }},
    {"drops buffers when closed, without reporting an error to the pusher", [](test_context &ctx) {
      elements::valve v("v");
      v.set_drop(true);
      ctx.check(v.is_dropping(), "is_dropping() should reflect set_drop(true)");

      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      v.get_static_pad("src")->link(downstream_sink);
      downstream_sink.set_active(true);

      int received = 0;
      downstream_sink.set_chain_function([&](pad &, buffer) {
        ++received;
        return flow_return::ok;
      });

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*v.get_static_pad("sink"));
      v.get_static_pad("sink")->set_active(true);

      ctx.check(upstream_src.push(buffer()) == flow_return::ok,
                 "a closed valve should report ok to the pusher, not an error");
      ctx.check_equal(received, 0);
    }},
  }) {}
};

inline static valve_test valve_test_instance;

} // namespace cxflow::testing
