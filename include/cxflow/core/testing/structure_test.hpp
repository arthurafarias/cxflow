// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/testing/test_group.hpp>
#include <cxflow/core/structure.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace cxflow::testing {

struct structure_test : public test_group {
  structure_test() : test_group("structure", {
    {"same name with no fields is compatible", [](test_context &ctx) {
      structure a("audio/x-raw");
      structure b("audio/x-raw");
      ctx.check(a.is_compatible_with(b), "same name with no fields should be compatible");
    }},
    {"different names are incompatible", [](test_context &ctx) {
      structure a("audio/x-raw");
      structure b("video/x-raw");
      ctx.check(!a.is_compatible_with(b), "different names should be incompatible");
    }},
    {"matching field values are compatible", [](test_context &ctx) {
      structure a("audio/x-raw");
      structure b("audio/x-raw");
      a.set("rate", std::int64_t{44100});
      b.set("rate", std::int64_t{44100});
      ctx.check(a.is_compatible_with(b), "matching field values should be compatible");
    }},
    {"conflicting field values are incompatible", [](test_context &ctx) {
      structure a("audio/x-raw");
      structure b("audio/x-raw");
      a.set("rate", std::int64_t{44100});
      b.set("rate", std::int64_t{48000});
      ctx.check(!a.is_compatible_with(b), "conflicting field values should be incompatible");
    }},
    {"a field present on only one side does not block the match", [](test_context &ctx) {
      structure a("audio/x-raw");
      structure b("audio/x-raw");
      a.set("rate", std::int64_t{44100});
      b.set("channels", std::int64_t{2});
      ctx.check(a.is_compatible_with(b), "a field present on only one side should not block the match");
      ctx.check(b.is_compatible_with(a), "compatibility should hold in both directions");
    }},
    {"get() returns nullopt for an absent field, the value for a present one", [](test_context &ctx) {
      structure s("audio/x-raw");
      ctx.check(!s.get("rate").has_value(), "an unset field should be absent");
      s.set("rate", std::int64_t{44100});
      auto v = s.get("rate");
      ctx.require(v.has_value(), "a set field should be present");
      ctx.check(std::get<std::int64_t>(*v) == 44100, "get() should return the value passed to set()");
    }},
    {"a negative field value round-trips exactly (OPEN-6)", [](test_context &ctx) {
      structure s("audio/x-raw");
      s.set("offset", std::int64_t{-1});
      auto v = s.get("offset");
      ctx.require(v.has_value(), "a set field should be present");
      ctx.check(std::get<std::int64_t>(*v) == -1, "a negative int64_t value should not wrap or be rejected");
    }},
    {"set() fires property_changed (REQ-5.4.3)", [](test_context &ctx) {
      structure s("audio/x-raw");
      std::vector<std::string> changed;
      s.property_changed.connect([&](const std::string &field) { changed.push_back(field); });
      s.set("rate", std::int64_t{44100});
      s.set("channels", std::int64_t{2});
      ctx.check(changed == std::vector<std::string>{"rate", "channels"}, "every set() after construction should notify property_changed with the field name");
    }},
  }) {}
};

inline static structure_test structure_test_instance;

} // namespace cxflow::testing
