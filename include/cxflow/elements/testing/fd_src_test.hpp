// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/fd_src.hpp>
#include <cxflow/testing/test_group.hpp>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include <fcntl.h>
#include <unistd.h>

namespace cxflow::testing {

struct fd_src_test : public test_group {
  fd_src_test() : test_group("fd_src", {
    {"reads an already-open fd's bytes into buffers, then sends eos", [](test_context &ctx) {
      auto path = std::filesystem::temp_directory_path() / "cxflow-fd_src_test.bin";
      {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << "hello, fd_src";
      }

      int descriptor = ::open(path.c_str(), O_RDONLY);
      ctx.require(descriptor >= 0, "opening the test fixture file should succeed");

      elements::fd_src src("src");
      src.set_fd(descriptor);
      src.set_blocksize(4);

      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      src.get_static_pad("src")->link(downstream_sink);
      downstream_sink.set_active(true);

      std::string collected;
      downstream_sink.set_chain_function([&](pad &, buffer buf) {
        auto data = buf.data();
        collected.append(reinterpret_cast<const char *>(data.data()), data.size());
        return flow_return::ok;
      });
      bool eos_seen = false;
      downstream_sink.set_event_function([&](pad &, const event &ev) {
        eos_seen = ev.type == event_type::eos;
        return true;
      });

      ctx.require(src.set_state(state::playing) == state_change_return::success, "reaching playing should succeed");
      for (int i = 0; i < 200 && !eos_seen; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      ctx.require(eos_seen, "fd_src should send eos once the fd is exhausted");
      ctx.check_equal(collected, std::string("hello, fd_src"));

      ctx.check(src.set_state(state::null) == state_change_return::success, "returning to null should succeed");

      ::close(descriptor); // fd_src never closes a caller-owned fd itself - see fd_src.hpp
      std::filesystem::remove(path);
    }},
    {"no fd set sends eos immediately instead of blocking", [](test_context &ctx) {
      elements::fd_src src("src");

      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      src.get_static_pad("src")->link(downstream_sink);
      downstream_sink.set_active(true);

      bool eos_seen = false;
      downstream_sink.set_event_function([&](pad &, const event &ev) {
        eos_seen = ev.type == event_type::eos;
        return true;
      });

      ctx.require(src.set_state(state::playing) == state_change_return::success, "reaching playing should succeed");
      for (int i = 0; i < 200 && !eos_seen; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      ctx.check(eos_seen, "no fd configured should behave as an already-exhausted source");

      ctx.check(src.set_state(state::null) == state_change_return::success, "returning to null should succeed");
    }},
  }) {}
};

inline static fd_src_test fd_src_test_instance;

} // namespace cxflow::testing
