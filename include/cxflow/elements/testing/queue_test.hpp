// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/queue.hpp>
#include <cxflow/testing/test_group.hpp>

#include <chrono>
#include <cstddef>
#include <thread>
#include <vector>

namespace cxflow::testing {

struct queue_test : public test_group {
  queue_test() : test_group("queue", {
    {"forwards buffers downstream, in order, on its own task thread", [](test_context &ctx) {
      elements::queue q("q");
      pad *sink_pad = q.get_static_pad("sink");
      pad *src_pad = q.get_static_pad("src");
      ctx.require(sink_pad != nullptr && src_pad != nullptr, "queue should expose sink and src pads");

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      ctx.require(upstream_src.link(*sink_pad), "linking upstream to queue's sink pad should succeed");

      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      ctx.require(src_pad->link(downstream_sink), "linking queue's src pad to downstream should succeed");
      downstream_sink.set_active(true);

      std::vector<int> received;
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        received.push_back(static_cast<int>(buf.offset));
        return flow_return::ok;
      });
      bool eos_seen = false;
      downstream_sink.set_event_function([&](pad &, const event &ev) {
        if (ev.type == event_type::eos) {
          eos_seen = true;
        }
        return true;
      });

      ctx.require(q.set_state(state::playing) == state_change_return::success, "reaching playing should succeed");

      for (int i = 0; i < 5; ++i) {
        buffer buf;
        buf.offset = static_cast<std::uint64_t>(i);
        upstream_src.push(std::move(buf));
      }
      upstream_src.send_event(event{event_type::eos});

      for (int i = 0; i < 200 && !eos_seen; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

      ctx.require(eos_seen, "eos queued after every buffer should still reach downstream");
      ctx.require_equal(received.size(), std::size_t{5});
      for (int i = 0; i < 5; ++i) {
        ctx.check_equal(received[static_cast<std::size_t>(i)], i);
      }

      ctx.check(q.set_state(state::null) == state_change_return::success, "returning to null should succeed");
    }},
    {"max-size-buffers bounds the queue by dropping the oldest item once full", [](test_context &ctx) {
      elements::queue q("q");
      q.set_max_size_buffers(2);
      pad *sink_pad = q.get_static_pad("sink");
      pad *src_pad = q.get_static_pad("src");

      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*sink_pad);

      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      src_pad->link(downstream_sink);
      downstream_sink.set_active(true);

      std::vector<int> received;
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        received.push_back(static_cast<int>(buf.offset));
        return flow_return::ok;
      });

      bool eos_seen = false;
      downstream_sink.set_event_function([&](pad &, const event &ev) {
        if (ev.type == event_type::eos) {
          eos_seen = true;
        }
        return true;
      });

      // Push all 5 before reaching playing, so the drain task hasn't
      // started yet and every push lands directly in the bounded internal
      // queue - deterministic, not racing the drain task.
      q.get_static_pad("sink")->set_active(true);
      for (int i = 0; i < 5; ++i) {
        buffer buf;
        buf.offset = static_cast<std::uint64_t>(i);
        upstream_src.push(std::move(buf));
      }
      ctx.check(received.empty(), "nothing should have drained before reaching playing");

      upstream_src.send_event(event{event_type::eos});
      ctx.require(q.set_state(state::playing) == state_change_return::success, "reaching playing should succeed");

      for (int i = 0; i < 200 && !eos_seen; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

      ctx.require(eos_seen, "eos should still reach downstream once draining starts");
      ctx.require_equal(received.size(), std::size_t{2});
      ctx.check_equal(received[0], 3);
      ctx.check_equal(received[1], 4);

      ctx.check(q.set_state(state::null) == state_change_return::success, "returning to null should succeed");
    }},
  }) {}
};

inline static queue_test queue_test_instance;

} // namespace cxflow::testing
