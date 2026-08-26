// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/core/plugin_registry.hpp>
#include <cxflow/core/structure.hpp>
#include <cxflow/elements/fake_sink.hpp>
#include <cxflow/elements/fake_src.hpp>
#include <cxflow/elements/identity.hpp>
#include <cxflow/testing/test_group.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cxflow::testing {

// SRS-002 §5.4 (REQ-5.4.1)/§10 M4: the fake_src/identity/fake_sink trio,
// grouped as one plugin via the self-registering-static-object idiom - the
// reference example for SRS-004's catalog entries. Registered here rather
// than inside elements/*.hpp itself: REQ-5.2.3 leaves adopting
// plugin_registry in the elements' own headers as an optional follow-on,
// not a breaking requirement, so every existing fake_src::register_type()
// -style call site keeps compiling/behaving identically (NFR-4).
// Construction below runs at static-init time, before any test case or
// example's own explicit register_type() call, and is idempotent with it -
// both ultimately call element_factory::register_type() for the same type
// name.
inline static plugin_registration fake_elements_plugin_registration{
    plugin_info{"cxflow-fake-elements", "Trivial source/passthrough/sink trio for exercising pipeline plumbing",
                "0.1.0", "Proprietary", "Arthur de Araújo Farias"},
    std::vector<element_factory_info>{
        element_factory_info{"fake_src",
                              [](std::string name) { return std::make_shared<elements::fake_src>(std::move(name)); },
                              rank::primary,
                              /*input_caps=*/{},
                              /*output_caps=*/{caps::any()}},
        element_factory_info{"identity",
                              [](std::string name) { return std::make_shared<elements::identity>(std::move(name)); },
                              rank::primary,
                              /*input_caps=*/{caps::any()},
                              /*output_caps=*/{caps::any()}},
        element_factory_info{"fake_sink",
                              [](std::string name) { return std::make_shared<elements::fake_sink>(std::move(name)); },
                              rank::primary,
                              /*input_caps=*/{caps::any()},
                              /*output_caps=*/{}},
    }};

struct plugin_registry_test : public test_group {
  plugin_registry_test() : test_group("plugin_registry", {
    {"rank values match GST_RANK_* numerically", [](test_context &ctx) {
      ctx.check_equal(static_cast<std::int32_t>(rank::none), std::int32_t{0});
      ctx.check_equal(static_cast<std::int32_t>(rank::marginal), std::int32_t{64});
      ctx.check_equal(static_cast<std::int32_t>(rank::secondary), std::int32_t{128});
      ctx.check_equal(static_cast<std::int32_t>(rank::primary), std::int32_t{256});
    }},
    {"register_plugin() makes the plugin discoverable via find_plugin()/all_plugins()", [](test_context &ctx) {
      plugin_registry::register_plugin(
          plugin_info{"plugin_registry_test.basic", "a test plugin", "1.0.0", "MIT", "test"},
          {element_factory_info{"plugin_registry_test.basic.type",
                                 [](std::string name) { return std::make_shared<element>(std::move(name)); }}});

      auto found = plugin_registry::find_plugin("plugin_registry_test.basic");
      ctx.require(found.has_value(), "find_plugin() should find the just-registered plugin");
      ctx.check_equal(found->description, std::string("a test plugin"));
      ctx.check_equal(found->version, std::string("1.0.0"));
      ctx.check_equal(found->license, std::string("MIT"));
      ctx.check_equal(found->author, std::string("test"));

      auto all = plugin_registry::all_plugins();
      bool present = std::any_of(all.begin(), all.end(),
                                  [](const plugin_info &p) { return p.name == "plugin_registry_test.basic"; });
      ctx.check(present, "all_plugins() should include the just-registered plugin");
    }},
    {"find_plugin() returns nullopt for an unregistered name", [](test_context &ctx) {
      auto found = plugin_registry::find_plugin("plugin_registry_test.does_not_exist");
      ctx.check(!found.has_value(), "find_plugin() for an unknown name should return nullopt");
    }},
    {"factories_of() returns a plugin's registered factories, still create()-able through element_factory",
     [](test_context &ctx) {
       plugin_registry::register_plugin(
           plugin_info{"plugin_registry_test.factories_of", "d", "1.0.0", "MIT", "test"},
           {element_factory_info{"plugin_registry_test.factories_of.type",
                                  [](std::string name) { return std::make_shared<element>(std::move(name)); }}});

       auto factories = plugin_registry::factories_of("plugin_registry_test.factories_of");
       ctx.require_equal(factories.size(), std::size_t{1});
       ctx.check_equal(factories[0].type_name, std::string("plugin_registry_test.factories_of.type"));

       // REQ-5.2.1: register_plugin() also registers with element_factory as a
       // side effect - create() by type name, unchanged, still works.
       auto instance = element_factory::create("plugin_registry_test.factories_of.type", "instance");
       ctx.require(instance != nullptr, "element_factory::create() should still work for a plugin-registered type");
     }},
    {"factories_of() for an unregistered plugin returns empty", [](test_context &ctx) {
      auto factories = plugin_registry::factories_of("plugin_registry_test.does_not_exist");
      ctx.check(factories.empty(), "factories_of() for an unknown plugin should return empty");
    }},
    {"factories_accepting()/factories_producing() order by descending rank, ties broken by registration order",
     [](test_context &ctx) {
       auto trivial_creator = [](std::string name) { return std::make_shared<element>(std::move(name)); };

       plugin_registry::register_plugin(
           plugin_info{"plugin_registry_test.ranked", "d", "1.0.0", "MIT", "test"},
           {
               element_factory_info{"plugin_registry_test.ranked.marginal", trivial_creator, rank::marginal,
                                     {caps::any()}, {caps::any()}},
               element_factory_info{"plugin_registry_test.ranked.primary_a", trivial_creator, rank::primary,
                                     {caps::any()}, {caps::any()}},
               element_factory_info{"plugin_registry_test.ranked.secondary", trivial_creator, rank::secondary,
                                     {caps::any()}, {caps::any()}},
               element_factory_info{"plugin_registry_test.ranked.primary_b", trivial_creator, rank::primary,
                                     {caps::any()}, {caps::any()}},
           });

       auto accepting = plugin_registry::factories_accepting(caps::any());
       std::vector<std::string> ranked_order;
       for (const auto &f : accepting) {
         if (f.type_name.starts_with("plugin_registry_test.ranked.")) {
           ranked_order.push_back(f.type_name);
         }
       }
       ctx.require_equal(ranked_order.size(), std::size_t{4});
       ctx.check_equal(ranked_order[0], std::string("plugin_registry_test.ranked.primary_a"));
       ctx.check_equal(ranked_order[1], std::string("plugin_registry_test.ranked.primary_b"));
       ctx.check_equal(ranked_order[2], std::string("plugin_registry_test.ranked.secondary"));
       ctx.check_equal(ranked_order[3], std::string("plugin_registry_test.ranked.marginal"));

       auto producing = plugin_registry::factories_producing(caps::any());
       std::vector<std::string> producing_order;
       for (const auto &f : producing) {
         if (f.type_name.starts_with("plugin_registry_test.ranked.")) {
           producing_order.push_back(f.type_name);
         }
       }
       ctx.check(producing_order == ranked_order,
                  "factories_producing() should apply the same rank ordering as factories_accepting()");
     }},
    {"factories_accepting() excludes a factory with no compatible input caps", [](test_context &ctx) {
      caps audio;
      audio.add(structure("audio/x-raw"));
      caps video;
      video.add(structure("video/x-raw"));

      plugin_registry::register_plugin(
          plugin_info{"plugin_registry_test.incompatible", "d", "1.0.0", "MIT", "test"},
          {element_factory_info{"plugin_registry_test.incompatible.type",
                                 [](std::string name) { return std::make_shared<element>(std::move(name)); },
                                 rank::primary, {audio}, {}}});

      auto matches = plugin_registry::factories_accepting(video);
      bool present = std::any_of(matches.begin(), matches.end(), [](const element_factory_info &f) {
        return f.type_name == "plugin_registry_test.incompatible.type";
      });
      ctx.check(!present, "factories_accepting() should not return a factory whose input caps don't match");
    }},
    {"the fake_src/identity/fake_sink trio round-trips through plugin_registry as one plugin", [](test_context &ctx) {
      auto found = plugin_registry::find_plugin("cxflow-fake-elements");
      ctx.require(found.has_value(), "the fake-elements plugin should have self-registered via static init");
      ctx.check_equal(found->version, std::string("0.1.0"));

      auto factories = plugin_registry::factories_of("cxflow-fake-elements");
      ctx.require_equal(factories.size(), std::size_t{3});

      for (const char *type_name : {"fake_src", "identity", "fake_sink"}) {
        bool present = std::any_of(factories.begin(), factories.end(),
                                    [&](const element_factory_info &f) { return f.type_name == type_name; });
        ctx.check(present, "factories_of() should list every factory registered for the plugin");

        auto instance = element_factory::create(type_name, "plugin_registry_test.round_trip_instance");
        ctx.check(instance != nullptr, "element_factory::create() should still work for a plugin-registered type");
      }
    }},
  }) {}
};

inline static plugin_registry_test plugin_registry_test_instance;

} // namespace cxflow::testing
