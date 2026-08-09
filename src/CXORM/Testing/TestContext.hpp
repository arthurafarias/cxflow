// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <CXORM/Core/Containers/Collection.hpp>
#include <CXORM/Core/Containers/String.hpp>

#include <sstream>
#include <source_location>

namespace CXORM::Testing {

// Passed into every test case body. Failures are recorded rather than
// thrown, so a case keeps running after a failed check (like GTest's
// EXPECT_*) instead of unwinding on the first mistake. require()/
// require_equal() return a bool for the rare case a test genuinely cannot
// continue past a failed check (e.g. indexing into a collection whose size
// just failed to match).
class TestContext {
public:
  bool check(bool condition, const char *description,
             const std::source_location &where = std::source_location::current()) {
    if (!condition) {
      fail(where, description);
    }
    return condition;
  }

  template <typename Actual, typename Expected>
  bool check_equal(const Actual &actual, const Expected &expected,
                    const char *label = "",
                    const std::source_location &where = std::source_location::current()) {
    bool equal = actual == expected;
    if (!equal) {
      std::ostringstream out;
      if (label && *label) {
        out << label << ": ";
      }
      out << "expected [" << expected << "] but got [" << actual << "]";
      fail(where, out.str());
    }
    return equal;
  }

  template <typename ExceptionType, typename Fn>
  bool check_throws(Fn &&fn, const char *label = "",
                    const std::source_location &where = std::source_location::current()) {
    try {
      fn();
    } catch (const ExceptionType &) {
      return true;
    } catch (const std::exception &e) {
      std::ostringstream out;
      out << label << ": threw the wrong exception type (" << e.what() << ")";
      fail(where, out.str());
      return false;
    }
    std::ostringstream out;
    out << label << ": expected an exception, none was thrown";
    fail(where, out.str());
    return false;
  }

  bool require(bool condition, const char *description,
              const std::source_location &where = std::source_location::current()) {
    return check(condition, description, where);
  }

  template <typename Actual, typename Expected>
  bool require_equal(const Actual &actual, const Expected &expected,
                     const char *label = "",
                     const std::source_location &where = std::source_location::current()) {
    return check_equal(actual, expected, label, where);
  }

  void fail(const std::source_location &where, const std::string &description) {
    std::ostringstream out;
    out << where.file_name() << ":" << where.line() << ": " << description;
    failures_.push_back(Core::Containers::String(out.str()));
  }

  bool failed() const { return !failures_.empty(); }
  const Core::Containers::Collection<Core::Containers::String> &failures() const {
    return failures_;
  }

private:
  Core::Containers::Collection<Core::Containers::String> failures_;
};

} // namespace CXORM::Testing
