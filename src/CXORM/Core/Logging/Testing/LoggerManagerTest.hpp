// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <CXORM/Core/Logging/LoggerManager.hpp>
#include <CXORM/Testing/TestGroup.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace CXORM::Core::Logging::Testing {

namespace {

// LoggerManager's stream and level are process-wide statics with no reset
// hook, so every non-isolated test below restores whatever was in place
// before it ran, to avoid leaking state (and log spam) into unrelated
// tests sharing this binary.
struct StreamGuard {
  std::shared_ptr<std::ostream> previous;
  std::shared_ptr<std::ostringstream> buffer;

  StreamGuard()
      : previous(LoggerManager::stream_current()),
        buffer(std::make_shared<std::ostringstream>()) {
    LoggerManager::stream_set(buffer);
  }

  ~StreamGuard() {
    LoggerManager::stream_set(previous);
    LoggerManager::level_set(LoggerManager::Level::Debug);
  }

  std::string logged() const { return buffer->str(); }
};

} // namespace

inline static ::CXORM::Testing::TestGroup LoggerManagerTest{
    "LoggerManager",
    {
        {"ErrorAlwaysLogsRegardlessOfLevel",
         [](auto &ctx) {
           StreamGuard guard;
           LoggerManager::level_set(LoggerManager::Level::Info);
           LoggerManager::error("boom {}", 1);
           ctx.check(guard.logged().find("ERROR: boom 1") != std::string::npos,
                     "expected the error message in the stream");
         }},

        {"InfoLogsAtTheDefaultInfoLevel",
         [](auto &ctx) {
           StreamGuard guard;
           LoggerManager::level_set(LoggerManager::Level::Info);
           LoggerManager::info("hello {}", 42);
           ctx.check(guard.logged().find("INFO: hello 42") != std::string::npos,
                     "expected the info message in the stream");
         }},

        {"WarningIsSuppressedBelowWarnLevel",
         [](auto &ctx) {
           StreamGuard guard;
           LoggerManager::level_set(LoggerManager::Level::Info);
           LoggerManager::warning("should not appear");
           ctx.check_equal(guard.logged(), std::string(""));
         }},

        {"WarningLogsAtWarnLevelOrAbove",
         [](auto &ctx) {
           StreamGuard guard;
           LoggerManager::level_set(LoggerManager::Level::Warn);
           LoggerManager::warning("careful");
           ctx.check(guard.logged().find("WARNING: careful") != std::string::npos,
                     "expected the warning message in the stream");
         }},

        {"DebugIsSuppressedBelowDebugLevel",
         [](auto &ctx) {
           StreamGuard guard;
           LoggerManager::level_set(LoggerManager::Level::Warn);
           LoggerManager::debug("should not appear");
           ctx.check_equal(guard.logged(), std::string(""));
         }},

        {"DebugLogsAtDebugLevel",
         [](auto &ctx) {
           StreamGuard guard;
           LoggerManager::level_set(LoggerManager::Level::Debug);
           LoggerManager::debug("verbose detail");
           ctx.check(guard.logged().find("DEBUG: verbose detail") != std::string::npos,
                     "expected the debug message in the stream");
         }},

        {"StreamSetIgnoresANullStreamRatherThanClearingCurrent",
         [](auto &ctx) {
           StreamGuard guard;
           LoggerManager::stream_set(nullptr);
           LoggerManager::info("still goes to the previous stream");
           ctx.check(guard.logged().find("still goes to the previous stream") !=
                         std::string::npos,
                     "a null stream_set() should be a no-op");
         }},

        // KNOWN DESIGN GAP: info()'s own suppression guard
        // (`if (_level < Level::Info) return;`) is unreachable through any
        // legitimate value of the public `Level` enum, because Info is
        // declared as the *lowest* enumerator (0). There is no "Off"/silent
        // tier below it, so there is no way to fully silence the logger via
        // the documented API - the guard only does anything if handed a
        // Level that was never one of the declared enumerators, as this
        // test does deliberately to prove the branch is live code, not to
        // recommend doing this in real usage.
        {"InfoGuardIsOnlyReachableWithAnUndeclaredLevelValue_KnownDesignGap",
         [](auto &ctx) {
           StreamGuard guard;
           LoggerManager::level_set(static_cast<LoggerManager::Level>(-1));
           LoggerManager::info("should not appear because there is no real Off level");
           ctx.check_equal(guard.logged(), std::string(""));
         }},

        // Deliberately its own process (isolated): the log-rotation branch
        // in LoggerManager::stream_file() ("if log.txt already exists, copy
        // it aside before creating a fresh one") only runs the first time
        // ANY log call forces the default stream to initialize, because
        // `_stream_default` is a process-lifetime static with no reset
        // hook. Inside a shared test binary that first call already
        // happened by the time any other test runs (the very first logged
        // call anywhere claims it) - running this scenario isolated is what
        // makes it deterministic instead of depending on test ordering.
        ::CXORM::Testing::TestCase{
            "BacksUpAnExistingLogFileBeforeCreatingANewOne",
            [](auto &ctx) {
              namespace fs = std::filesystem;

              fs::remove("log.txt");
              for (auto &entry : fs::directory_iterator(fs::current_path())) {
                auto name = entry.path().filename().string();
                if (name.starts_with("log.") && name != "log.txt") {
                  fs::remove(entry.path());
                }
              }

              {
                std::ofstream preexisting("log.txt");
                preexisting << "leftover from a previous run\n";
              }

              LoggerManager::info("first message after startup");

              bool backup_found = false;
              for (auto &entry : fs::directory_iterator(fs::current_path())) {
                auto name = entry.path().filename().string();
                if (name.starts_with("log.") && name != "log.txt") {
                  backup_found = true;
                }
              }

              ctx.check(backup_found,
                        "expected stream_file() to have backed up the pre-existing log.txt");
              ctx.check(fs::exists("log.txt"), "expected a fresh log.txt to exist");
            }}
            .isolated(),
    }};

} // namespace CXORM::Core::Logging::Testing
