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
#include <cxflow/elements/file_src.hpp>
#include <cxflow/testing/test_group.hpp>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace cxflow::testing {

struct file_src_test : public test_group {
  file_src_test() : test_group("file_src", {
    {"reads a file's bytes into buffers, then sends eos", [](test_context &ctx) {
      auto path = std::filesystem::temp_directory_path() / "cxflow-file_src_test.bin";
      {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << "hello, file_src";
      }

      elements::file_src src("src");
      src.set_location(path.string());
      src.set_blocksize(4); // force multiple buffers for a short file

      element downstream("downstream");
      pad &downstream_sink = downstream.add_pad(std::make_unique<pad>("sink", pad::direction::sink, downstream));
      ctx.require(src.get_static_pad("src")->link(downstream_sink), "linking file_src should succeed");
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
      ctx.require(eos_seen, "file_src should send eos once the file is exhausted");
      ctx.check_equal(collected, std::string("hello, file_src"));

      ctx.check(src.set_state(state::null) == state_change_return::success, "returning to null should succeed");
      std::filesystem::remove(path);
    }},
    {"a missing file fails the null->ready transition and posts a bus error", [](test_context &ctx) {
      auto src = std::make_shared<elements::file_src>("src");
      src->set_location("/nonexistent/path/that/should/never/exist/cxflow.bin");

      pipeline pipe("pipe");
      pipe.add(src);

      ctx.check(src->set_state(state::ready) == state_change_return::failure,
                 "opening a nonexistent file should fail the state transition");

      auto msg = pipe.bus().pop(std::chrono::milliseconds(0));
      ctx.require(msg.has_value(), "a failed open should post a bus message");
      ctx.check(msg->type == message_type::error, "the posted message should be an error");
    }},
  }) {}
};

inline static file_src_test file_src_test_instance;

} // namespace cxflow::testing
