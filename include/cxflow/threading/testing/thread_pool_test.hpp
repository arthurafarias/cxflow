// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/testing/test_group.hpp>
#include <cxflow/threading/thread_pool.hpp>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace cxflow::testing {

struct thread_pool_test : public test_group {
  thread_pool_test() : test_group("thread_pool", {
    {"submit() executes the task asynchronously", [](test_context &ctx) {
      threading::thread_pool pool(2);
      std::atomic<bool> ran{false};
      pool.submit([&] { ran = true; });

      for (int i = 0; i < 200 && !ran.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      ctx.check(ran.load(), "the submitted task should have run");
    }},
    {"multiple submissions all run", [](test_context &ctx) {
      threading::thread_pool pool(4);
      constexpr int total = 20;
      std::atomic<int> count{0};
      for (int i = 0; i < total; ++i) {
        pool.submit([&] { ++count; });
      }

      for (int i = 0; i < 200 && count.load() < total; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      ctx.check_equal(count.load(), total);
    }},
    {"an exception thrown by one task does not break the worker or later submissions", [](test_context &ctx) {
      threading::thread_pool pool(2);
      pool.submit([] { throw std::runtime_error("boom"); });

      std::atomic<bool> ran{false};
      pool.submit([&] { ran = true; });

      for (int i = 0; i < 200 && !ran.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      ctx.check(ran.load(), "a later submission should still run after a prior task threw");
    }},
    {"instance() returns the same reference across calls", [](test_context &ctx) {
      ctx.check(&threading::thread_pool::instance() == &threading::thread_pool::instance(), "instance() should return the same reference across calls");
    }},
  }) {}
};

inline static thread_pool_test thread_pool_test_instance;

} // namespace cxflow::testing
