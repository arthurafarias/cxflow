// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <CXORM/Testing/Registry.hpp>
#include <CXORM/Testing/TestCase.hpp>

#include <initializer_list>
#include <vector>

namespace CXORM::Testing {

// One TestGroup per class, declared as an `inline static` in that class's
// sibling Testing/<Class>Test.hpp (namespace CXORM::...::Testing).
// Construction self-registers the group into the global Registry, so the
// generic runner in main.cpp needs no knowledge of what classes exist - it
// just walks whatever registered itself before main() ran.
class TestGroup {
public:
  TestGroup(Core::Containers::String name, std::initializer_list<TestCase> cases)
      : name_(std::move(name)), cases_(cases) {
    Registry::instance().add(this);
  }

  const Core::Containers::String &name() const { return name_; }
  const std::vector<TestCase> &cases() const { return cases_; }

private:
  Core::Containers::String name_;
  std::vector<TestCase> cases_;
};

} // namespace CXORM::Testing
