// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include "Core/SharedPointer.hpp"
#include "Core/Threading/Mutex.hpp"
#include "Core/Threading/UniqueLock.hpp"

#include <memory>

namespace Core {
class Object {
public:
  Object() : _mutex(SharedPointer<Mutex>(new Mutex())) {}
  virtual ~Object() {}

  std::shared_ptr<Mutex> mutex() { return _mutex; }

  UniqueLock<Mutex> acquire_lock() const { return UniqueLock<Mutex>(*_mutex); }

private:
  SharedPointer<Mutex> _mutex;
};
} // namespace Core