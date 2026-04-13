// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#ifndef _threaded_signals_hpp_
#define _threaded_signals_hpp_

#include "Core/Threading/Mutex.hpp"
#include "Core/Threading/UniqueLock.hpp"
#include <functional>
#include <list>
#include <mutex>

#include <Core/Logging/LoggerManager.hpp>
#include <Core/Threading/ThreadPool.hpp>

namespace CXORM::Core::Threading {

template <typename sender_type, typename... args_types> class Signal {
public:
  Signal() = default;

  Signal(Signal &) = delete;
  Signal &operator=(Signal &) = delete;

  Signal(Signal &&) = delete;
  Signal &operator=(Signal &&) = delete;

  ~Signal() { disconnect_all(); }

  using SlotType = std::function<void(sender_type, args_types...)>;
  using HandleType = std::list<SlotType>::iterator;

  const std::list<SlotType>::iterator connect(const SlotType &slot) {
    UniqueLock<Mutex> lock(_mutex);

    Core::Logging::LoggerManager::debug("Core::Threading::signal", __func__);

    return _sinks.insert(_sinks.end(), std::move(slot));
  }

  void disconnect(const std::list<SlotType>::iterator &it) {
    std::unique_lock<std::mutex> lock(_mutex);

    Core::Logging::LoggerManager::debug("{}: {}", __func__, "");

    _sinks.erase(it);
  }

  void disconnect_all() {
    std::unique_lock<std::mutex> lock(_mutex);

    Core::Logging::LoggerManager::debug("{}: {}", __func__, "");

    _sinks.clear();
  }

  void operator()(sender_type sender, args_types... args) {
    std::unique_lock<std::mutex> lock(_mutex);

    Core::Logging::LoggerManager::debug("{}: {}", __func__, "");

    for (const auto &slot : _sinks) {

      Core::Logging::LoggerManager::debug("{}: {}", __func__, "");

      _thread_pool.submit([slot, sender, args...]() {
        Core::Logging::LoggerManager::debug(__func__, "begin slot call");

        slot(sender, args...);

        Core::Logging::LoggerManager::debug("{}: {}", __func__,
                                            "end slog call");
      });
    }
  }

  Signal &operator+=(const SlotType &slot) {
    connect(slot);
    return *this;
  }

  Signal &operator-=(const std::list<SlotType>::iterator &it) {
    disconnect(it);
    return *this;
  }

  const auto &sinks() { return _sinks; }

protected:
  std::mutex _mutex;
  std::list<SlotType> _sinks;
  ThreadPool &_thread_pool = ThreadPool::get_instance();
};
} // namespace CXORM::Core::Threading

#endif