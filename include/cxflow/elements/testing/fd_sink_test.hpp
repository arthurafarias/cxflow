// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/fd_sink.hpp>
#include <cxflow/testing/test_group.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace cxflow::testing {

struct fd_sink_test : public test_group {
  fd_sink_test() : test_group("fd_sink", {
    {"writes every buffer's bytes to the fd, in order", [](test_context &ctx) {
      auto path = std::filesystem::temp_directory_path() / "cxflow-fd_sink_test.bin";
      std::filesystem::remove(path);

      int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
      ctx.require(descriptor >= 0, "opening the destination file for writing should succeed");

      elements::fd_sink sink("sink");
      sink.set_fd(descriptor);

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

      ctx.check(upstream_src.push(make_buffer("hello, ")) == flow_return::ok, "pushing should succeed");
      ctx.check(upstream_src.push(make_buffer("fd_sink")) == flow_return::ok, "pushing should succeed");

      ::close(descriptor); // fd_sink never closes a caller-owned fd itself - see fd_sink.hpp

      std::ifstream in(path, std::ios::binary);
      std::ostringstream contents;
      contents << in.rdbuf();
      ctx.check_equal(contents.str(), std::string("hello, fd_sink"));

      std::filesystem::remove(path);
    }},
    {"no fd set reports an error instead of writing nowhere silently", [](test_context &ctx) {
      elements::fd_sink sink("sink");
      pad *sink_pad = sink.get_static_pad("sink");
      element upstream("upstream");
      pad &upstream_src = upstream.add_pad(std::make_unique<pad>("src", pad::direction::src, upstream));
      upstream_src.link(*sink_pad);
      sink_pad->set_active(true);

      ctx.check(upstream_src.push(buffer()) == flow_return::error, "no fd configured should report an error");
    }},
  }) {}
};

inline static fd_sink_test fd_sink_test_instance;

} // namespace cxflow::testing
