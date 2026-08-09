// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <vector>

namespace CXORM::Testing {

class TestGroup;

// Meyer's singleton: guarantees the registry exists before any `inline
// static TestGroup` in any Testing/*.hpp registers itself, regardless of
// static-initialization order across translation units.
class Registry {
public:
  static Registry &instance() {
    static Registry registry;
    return registry;
  }

  void add(TestGroup *group) { groups_.push_back(group); }
  const std::vector<TestGroup *> &groups() const { return groups_; }

private:
  Registry() = default;
  std::vector<TestGroup *> groups_;
};

} // namespace CXORM::Testing
