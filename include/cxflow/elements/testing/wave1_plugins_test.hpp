// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/elements/wave1_plugins.hpp>
#include <cxflow/testing/test_group.hpp>

#include <algorithm>
#include <array>
#include <string>

namespace cxflow::testing {

struct wave1_plugins_test : public test_group {
  wave1_plugins_test() : test_group("wave1_plugins", {
    {"every Wave 1 entry is discoverable via plugin_registry and still create()-able through element_factory",
     [](test_context &ctx) {
       static constexpr std::array<std::pair<const char *, const char *>, 14> entries{{
           {"cxflow-io-elements", "file_src"},
           {"cxflow-io-elements", "file_sink"},
           {"cxflow-io-elements", "fd_src"},
           {"cxflow-io-elements", "fd_sink"},
           {"cxflow-io-elements", "app_src"},
           {"cxflow-io-elements", "app_sink"},
           {"cxflow-utility-elements", "queue"},
           {"cxflow-utility-elements", "tee"},
           {"cxflow-utility-elements", "valve"},
           {"cxflow-utility-elements", "caps_filter"},
           {"cxflow-container-elements", "wav_mux"},
           {"cxflow-container-elements", "wav_demux"},
           {"cxflow-container-elements", "au_mux"},
           {"cxflow-container-elements", "au_demux"},
       }};

       for (const auto &[plugin_name, type_name] : entries) {
         auto plugin = plugin_registry::find_plugin(plugin_name);
         ctx.require(plugin.has_value(), (std::string("plugin '") + plugin_name + "' should be registered").c_str());

         auto factories = plugin_registry::factories_of(plugin_name);
         bool listed = std::any_of(factories.begin(), factories.end(),
                                    [&](const element_factory_info &f) { return f.type_name == type_name; });
         ctx.check(listed, (std::string("'") + type_name + "' should be listed under its plugin").c_str());

         auto instance = element_factory::create(type_name, std::string("wave1_plugins_test.") + type_name);
         ctx.check(instance != nullptr,
                    (std::string("element_factory::create() should still work for '") + type_name + "'").c_str());
       }
     }},
  }) {}
};

inline static wave1_plugins_test wave1_plugins_test_instance;

} // namespace cxflow::testing
