// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/testing/test_group.hpp>
#include <cxflow/core/bus.hpp>

#include <chrono>
#include <string>

namespace cxflow::testing {

struct bus_test : public test_group {
  bus_test() : test_group("bus", {
    {"post()/pop() is FIFO", [](test_context &ctx) {
      bus b;
      message first;
      first.type = message_type::info;
      first.debug_info = "first";
      message second;
      second.type = message_type::warning;
      second.debug_info = "second";

      b.post(first);
      b.post(second);

      auto popped_first = b.pop(std::chrono::milliseconds(0));
      auto popped_second = b.pop(std::chrono::milliseconds(0));
      ctx.require(popped_first.has_value(), "the first pop() should return a message");
      ctx.require(popped_second.has_value(), "the second pop() should return a message");
      ctx.check_equal(popped_first->debug_info, std::string("first"));
      ctx.check_equal(popped_second->debug_info, std::string("second"));
    }},
    {"pop() times out with nullopt on an empty queue", [](test_context &ctx) {
      bus b;
      auto result = b.pop(std::chrono::milliseconds(10));
      ctx.check(!result.has_value());
    }},
    {"message_posted fires with the posted message", [](test_context &ctx) {
      bus b;
      int count = 0;
      message_type seen_type{};
      b.message_posted.connect([&](bus &, const message &msg) {
        ++count;
        seen_type = msg.type;
      });

      message msg;
      msg.type = message_type::error;
      b.post(msg);

      ctx.check_equal(count, 1);
      ctx.check(seen_type == message_type::error);
    }},
  }) {}
};

inline static bus_test bus_test_instance;

} // namespace cxflow::testing
