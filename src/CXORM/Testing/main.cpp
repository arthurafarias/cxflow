// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include <CXORM/Core/Containers/String.hpp>
#include <CXORM/Testing/Registry.hpp>
#include <CXORM/Testing/TestGroup.hpp>

#include <format>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

using namespace CXORM::Testing;
using namespace CXORM::Core::Containers;

namespace {

// Runs exactly one named case in-process and reports its result via exit
// code. This is what a subprocess spawned by run_isolated() below actually
// executes when this same binary is re-invoked with `--run group::case`.
int run_single(const String &group_name, const String &case_name) {
  for (auto *group : Registry::instance().groups()) {
    if (group->name() != group_name) {
      continue;
    }
    for (const auto &test_case : group->cases()) {
      if (test_case.name() != case_name) {
        continue;
      }
      TestContext ctx;
      test_case.run(ctx);
      return ctx.failed() ? 1 : 0;
    }
  }
  std::cerr << "no such test: " << group_name.c_str() << "::" << case_name.c_str() << "\n";
  return 2;
}

// Re-execs this same binary for one test case, so a genuinely fresh process
// runs it: fork() alone would just copy this process's already-initialized
// statics, which defeats the purpose for tests that need pristine global
// state or that exercise real undefined behavior. Plain fork()+execv()
// (rather than going through a shell) means a child killed by a signal
// reports directly via WIFSIGNALED on its wait status.
bool run_isolated(const String &executable, const String &group_name,
                  const String &case_name, bool expect_crash) {
  String combined = std::format("{}::{}", group_name.c_str(), case_name.c_str());

  pid_t child = fork();
  if (child == 0) {
    execl(executable.c_str(), executable.c_str(), "--run", combined.c_str(),
          static_cast<char *>(nullptr));
    _exit(127); // execl only returns on failure
  }

  int status = 0;
  waitpid(child, &status, 0);

  bool crashed = WIFSIGNALED(status);
  bool exited_cleanly = WIFEXITED(status) && WEXITSTATUS(status) == 0;

  return expect_crash ? crashed : exited_cleanly;
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 3 && String(argv[1]) == "--run") {
    String arg = argv[2];
    auto separator = arg.find("::");
    return run_single(arg.substr(0, separator), arg.substr(separator + 2));
  }

  int total = 0;
  int passed = 0;

  for (auto *group : Registry::instance().groups()) {
    for (const auto &test_case : group->cases()) {
      ++total;
      bool ok = false;
      Collection<String> failures;

      if (test_case.needs_subprocess()) {
        ok = run_isolated(argv[0], group->name(), test_case.name(),
                          test_case.expects_crash());
      } else {
        TestContext ctx;
        try {
          test_case.run(ctx);
        } catch (const std::exception &e) {
          ctx.fail(std::source_location::current(),
                   std::format("uncaught exception: {}", e.what()));
        } catch (...) {
          ctx.fail(std::source_location::current(), "uncaught non-exception value");
        }
        ok = !ctx.failed();
        failures = ctx.failures();
      }

      std::cout << (ok ? "[  OK  ] " : "[ FAIL ] ") << group->name().c_str()
                << "::" << test_case.name().c_str() << "\n";
      for (const auto &failure : failures) {
        std::cout << "         " << failure.c_str() << "\n";
      }

      passed += ok ? 1 : 0;
    }
  }

  std::cout << "\n" << passed << "/" << total << " tests passed\n";
  return passed == total ? 0 : 1;
}
