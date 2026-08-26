// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/file_sink.hpp>
#include <cxflow/testing/test_group.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace cxflow::testing {

struct file_sink_test : public test_group {
  file_sink_test() : test_group("file_sink", {
    {"writes every buffer's bytes to the file, in order", [](test_context &ctx) {
      auto path = std::filesystem::temp_directory_path() / "cxflow-file_sink_test.bin";
      std::filesystem::remove(path);

      elements::file_sink sink("sink");
      sink.set_location(path.string());

      ctx.require(sink.set_state(state::playing) == state_change_return::success, "reaching playing should succeed");

      pad *sink_pad = sink.get_static_pad("sink");
      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*sink_pad);
      sink_pad->set_active(true);

      auto make_buffer = [](const std::string &text) {
        std::vector<std::byte> bytes(text.size());
        for (std::size_t i = 0; i < text.size(); ++i) {
          bytes[i] = static_cast<std::byte>(text[i]);
        }
        return buffer(std::move(bytes));
      };

      upstream_src.push(make_buffer("hello, "));
      upstream_src.push(make_buffer("file_sink"));
      upstream_src.send_event(event{event_type::eos});

      ctx.require(sink.set_state(state::null) == state_change_return::success, "returning to null should succeed");

      std::ifstream in(path, std::ios::binary);
      std::ostringstream contents;
      contents << in.rdbuf();
      ctx.check_equal(contents.str(), std::string("hello, file_sink"));

      std::filesystem::remove(path);
    }},
    {"an unwritable location fails the null->ready transition", [](test_context &ctx) {
      elements::file_sink sink("sink");
      sink.set_location("/nonexistent-directory-cxflow/should-fail.bin");
      ctx.check(sink.set_state(state::ready) == state_change_return::failure,
                 "opening a path in a nonexistent directory should fail the state transition");
    }},
  }) {}
};

inline static file_sink_test file_sink_test_instance;

} // namespace cxflow::testing
