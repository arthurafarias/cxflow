// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <CXORM/Core/Containers/String.hpp>
#include <CXORM/Testing/TestContext.hpp>

#include <functional>
#include <utility>

namespace CXORM::Testing {

class TestCase {
public:
  using Body = std::function<void(TestContext &)>;

  TestCase(Core::Containers::String name, Body body)
      : name_(std::move(name)), body_(std::move(body)) {}

  // Runs in a freshly exec'd child process, for cases that depend on
  // process-lifetime global state (e.g. LoggerManager's default stream)
  // being untouched by any earlier case in the same binary.
  TestCase &isolated() {
    isolated_ = true;
    return *this;
  }

  // Runs in a freshly exec'd child process and PASSES only if that process
  // terminates abnormally (a signal, or an uncaught-exception abort) rather
  // than returning normally - for exercising genuine undefined-behavior
  // crashes without taking the whole test binary down with them.
  TestCase &expect_crash() {
    isolated_ = true;
    expect_crash_ = true;
    return *this;
  }

  const Core::Containers::String &name() const { return name_; }
  bool needs_subprocess() const { return isolated_; }
  bool expects_crash() const { return expect_crash_; }
  void run(TestContext &ctx) const { body_(ctx); }

private:
  Core::Containers::String name_;
  Body body_;
  bool isolated_ = false;
  bool expect_crash_ = false;
};

} // namespace CXORM::Testing
