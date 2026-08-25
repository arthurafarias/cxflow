// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <cxflow/core/element.hpp>

namespace cxflow {

// Static in-process name -> creator registry. Later modules register their
// own element types here without the core knowing about them upfront -
// deliberately lighter than GStreamer's plugin/.so registry, since dynamic
// loading is out of scope for this pass.
class element_factory {
public:
  using creator_function = std::function<std::shared_ptr<element>(std::string name)>;

  static void register_type(std::string type_name, creator_function creator);
  static std::shared_ptr<element> create(const std::string &type_name, std::string instance_name);
};

namespace detail {

// A named (not anonymous) namespace is required here: this registry backs
// element_factory's "one process-wide name -> creator map" contract, so
// every translation unit's register_type()/create() calls must resolve to
// the same instance. An anonymous namespace would give each TU its own
// internal-linkage copy of get_element_factory_registry(), silently
// breaking cross-module registration (e.g. fake_sink.cpp registering into a
// registry a caller in a different TU never sees). `inline` on a function
// with a function-local static guarantees a single instance program-wide.
struct element_factory_registry {
  std::mutex mutex;
  std::map<std::string, element_factory::creator_function> creators;
};

inline element_factory_registry &get_element_factory_registry() {
  static element_factory_registry instance;
  return instance;
}

} // namespace detail

inline void element_factory::register_type(std::string type_name, creator_function creator) {
  detail::element_factory_registry &r = detail::get_element_factory_registry();
  std::unique_lock lock(r.mutex);
  r.creators[std::move(type_name)] = std::move(creator);
}

inline std::shared_ptr<element> element_factory::create(const std::string &type_name, std::string instance_name) {
  detail::element_factory_registry &r = detail::get_element_factory_registry();
  std::unique_lock lock(r.mutex);

  auto it = r.creators.find(type_name);
  if (it == r.creators.end()) {
    return nullptr;
  }

  auto creator = it->second;
  lock.unlock();

  return creator(std::move(instance_name));
}

} // namespace cxflow
