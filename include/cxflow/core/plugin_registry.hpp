// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <algorithm>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <cxflow/core/caps.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/plugin.hpp>
#include <cxflow/logging/journal.hpp>

namespace cxflow {

// SRS-002 §5.2: a layer of metadata/grouping/rank on top of element_factory
// (§4's flat name -> creator map), not a replacement for it - every factory
// registered here is also registered with element_factory::register_type()
// (REQ-5.2.1), so create() by type name keeps working unchanged (NFR-4).
class plugin_registry {
public:
  static void register_plugin(plugin_info info, std::vector<element_factory_info> factories);

  static std::optional<plugin_info> find_plugin(const std::string &name);
  static std::vector<plugin_info> all_plugins();
  static std::vector<element_factory_info> factories_of(const std::string &plugin_name);

  // SRS-002 §5.5 (autoplugging): matches ordered by descending rank, ties
  // broken by registration order (REQ-5.5.1), using caps::is_compatible_with
  // unchanged (REQ-5.5.2) - no second compatibility algorithm.
  static std::vector<element_factory_info> factories_accepting(const caps &input);
  static std::vector<element_factory_info> factories_producing(const caps &output);
};

// SRS-002 §5.4 (REQ-5.4.1): the same self-registering-static-object idiom
// this codebase already uses for tests (testing/test_group.hpp) - a plugin
// header declares `inline static plugin_registration x{info, factories};`
// at namespace scope, and construction (during static initialization,
// before main()) registers it. No macro system, no code generation.
class plugin_registration {
public:
  plugin_registration(plugin_info info, std::vector<element_factory_info> factories) {
    plugin_registry::register_plugin(std::move(info), std::move(factories));
  }
};

namespace detail {

// Mirrors element_factory_registry's own reasoning (element_factory.hpp): a
// named namespace and a function-local static, so every translation unit's
// plugin_registry calls resolve to the same process-wide instance.
struct plugin_registry_state {
  std::mutex mutex;
  std::map<std::string, plugin_info> plugins;
  std::map<std::string, std::vector<element_factory_info>> factories_by_plugin;

  // Flat, append-only view of every factory ever registered, in the order
  // register_plugin() was called - factories_by_plugin is keyed by plugin
  // name (map order, not registration order), so §5.5's tie-breaking needs
  // this separate list. A plugin is expected to register exactly once via
  // §5.4's static-registration idiom, so re-registering the same plugin name
  // (not exercised by this SRS's acceptance criteria) would leave this list
  // holding both the old and new factories rather than replacing them.
  std::vector<element_factory_info> factories_in_registration_order;
};

inline plugin_registry_state &get_plugin_registry_state() {
  static plugin_registry_state instance;
  return instance;
}

} // namespace detail

inline void plugin_registry::register_plugin(plugin_info info, std::vector<element_factory_info> factories) {
  detail::plugin_registry_state &r = detail::get_plugin_registry_state();

  std::string plugin_name = info.name;
  std::size_t factory_count = factories.size();

  {
    std::unique_lock lock(r.mutex);
    r.factories_in_registration_order.insert(r.factories_in_registration_order.end(), factories.begin(),
                                              factories.end());
    r.factories_by_plugin[plugin_name] = factories;
    r.plugins[plugin_name] = std::move(info);
  }

  journal::debug("plugin_registry registered plugin '{}' with {} factories", plugin_name, factory_count);

  // REQ-5.2.1: side-effect registration with the existing element_factory,
  // outside the lock above - matches element_factory::create()'s own
  // discipline of not holding its registry mutex while invoking other code.
  for (const element_factory_info &f : factories) {
    element_factory::register_type(f.type_name, f.creator);
  }
}

inline std::optional<plugin_info> plugin_registry::find_plugin(const std::string &name) {
  detail::plugin_registry_state &r = detail::get_plugin_registry_state();
  std::unique_lock lock(r.mutex);

  auto it = r.plugins.find(name);
  if (it == r.plugins.end()) {
    return std::nullopt;
  }
  return it->second;
}

inline std::vector<plugin_info> plugin_registry::all_plugins() {
  detail::plugin_registry_state &r = detail::get_plugin_registry_state();
  std::unique_lock lock(r.mutex);

  std::vector<plugin_info> result;
  result.reserve(r.plugins.size());
  for (const auto &[name, info] : r.plugins) {
    result.push_back(info);
  }
  return result;
}

inline std::vector<element_factory_info> plugin_registry::factories_of(const std::string &plugin_name) {
  detail::plugin_registry_state &r = detail::get_plugin_registry_state();
  std::unique_lock lock(r.mutex);

  auto it = r.factories_by_plugin.find(plugin_name);
  if (it == r.factories_by_plugin.end()) {
    return {};
  }
  return it->second;
}

namespace detail {

// Shared by factories_accepting()/factories_producing(): pick every factory
// whose caps list (input_caps or output_caps, per caps_of) has at least one
// entry compatible with the query, in registration order, then stable-sort
// by descending rank so equal-rank matches keep that registration order
// (REQ-5.5.1).
template <typename CapsOf>
inline std::vector<element_factory_info> plugin_registry_matching(const caps &query, CapsOf caps_of) {
  plugin_registry_state &r = get_plugin_registry_state();

  std::vector<element_factory_info> candidates;
  {
    std::unique_lock lock(r.mutex);
    for (const element_factory_info &f : r.factories_in_registration_order) {
      const std::vector<caps> &side = caps_of(f);
      bool matches = std::any_of(side.begin(), side.end(),
                                  [&](const caps &c) { return c.is_compatible_with(query); });
      if (matches) {
        candidates.push_back(f);
      }
    }
  }

  std::stable_sort(candidates.begin(), candidates.end(), [](const element_factory_info &a,
                                                              const element_factory_info &b) {
    return a.factory_rank > b.factory_rank;
  });
  return candidates;
}

} // namespace detail

inline std::vector<element_factory_info> plugin_registry::factories_accepting(const caps &input) {
  return detail::plugin_registry_matching(input, [](const element_factory_info &f) -> const std::vector<caps> & {
    return f.input_caps;
  });
}

inline std::vector<element_factory_info> plugin_registry::factories_producing(const caps &output) {
  return detail::plugin_registry_matching(output, [](const element_factory_info &f) -> const std::vector<caps> & {
    return f.output_caps;
  });
}

} // namespace cxflow
