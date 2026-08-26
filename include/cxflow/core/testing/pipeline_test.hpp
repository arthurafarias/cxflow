// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/testing/test_group.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/pipeline.hpp>
#include <cxflow/core/pipeline_parser.hpp>
#include <cxflow/elements/fake_sink.hpp>
#include <cxflow/elements/fake_src.hpp>
#include <cxflow/elements/identity.hpp>

#include <chrono>
#include <cstdint>
#include <memory>

namespace cxflow::testing {

struct pipeline_test : public test_group {
  pipeline_test() : test_group("pipeline", {
    {"constructs with a non-null bus", [](test_context &ctx) {
      pipeline p("p");
      ctx.check(p.element::bus() != nullptr, "a pipeline should construct with a non-null bus");
    }},
    {"bus() hides element::bus(), returning the same underlying bus", [](test_context &ctx) {
      pipeline p("p");
      ctx.check(&p.bus() == p.element::bus().get(), "bus() should return the same underlying bus as element::bus()");
    }},
    {"a child added to the pipeline can post through its bus end to end", [](test_context &ctx) {
      pipeline p("pipe");
      auto child = std::make_shared<element>("child");
      p.add(child);

      message msg;
      msg.type = message_type::eos;
      child->post_message(msg);

      auto popped = p.bus().pop(std::chrono::milliseconds(0));
      ctx.require(popped.has_value(), "posting through the child should reach the pipeline's bus");
      ctx.check(popped->type == message_type::eos, "the popped message should be of type eos");
    }},
    {"to_variant()/from_variant() round-trips a multi-element, linked pipeline with non-default properties",
     [](test_context &ctx) {
       // SRS-003 REQ-5.3.3/§9 acceptance item 4.
       elements::fake_src::register_type();
       elements::identity::register_type();
       elements::fake_sink::register_type();

       auto original = pipeline_parser::parse("fake_src name=src num-buffers=7 ! identity name=id ! fake_sink name=sink");
       ctx.require(original.has_value(), "the source pipeline should parse successfully");

       auto restored = pipeline::from_variant((*original)->to_variant());
       ctx.require(restored != nullptr, "from_variant() should reconstruct a pipeline");
       ctx.require_equal(restored->children().size(), std::size_t{3});

       for (std::size_t i = 0; i < 3; ++i) {
         ctx.check_equal(restored->children()[i]->name(), (*original)->children()[i]->name());
         ctx.check_equal(restored->children()[i]->registered_type_name(),
                           (*original)->children()[i]->registered_type_name());
       }

       ctx.check_equal(restored->children()[0]->property_get<std::int64_t>("num-buffers").value_or(-99),
                         std::int64_t{7});

       pad *restored_src_out = restored->children()[0]->get_static_pad("src");
       pad *restored_id_in = restored->children()[1]->get_static_pad("sink");
       pad *restored_id_out = restored->children()[1]->get_static_pad("src");
       pad *restored_sink_in = restored->children()[2]->get_static_pad("sink");
       ctx.require(restored_src_out != nullptr && restored_id_in != nullptr && restored_id_out != nullptr &&
                        restored_sink_in != nullptr,
                    "every restored element should expose the expected pad");
       ctx.check(restored_src_out->peer() == restored_id_in, "the src->identity link should survive the round trip");
       ctx.check(restored_id_out->peer() == restored_sink_in,
                  "the identity->sink link should survive the round trip");
     }},
    {"from_variant() defaults to an empty pipeline when 'children' is absent", [](test_context &ctx) {
      containers::variant description = std::map<std::string, containers::variant>{
          {"name", containers::variant(std::string("empty-pipe"))}};

      auto restored = pipeline::from_variant(description);
      ctx.require(restored != nullptr, "from_variant() should still construct a pipeline");
      ctx.check_equal(restored->name(), std::string("empty-pipe"));
      ctx.check(restored->children().empty(), "a description with no 'children' key should produce no children");
    }},
  }) {}
};

inline static pipeline_test pipeline_test_instance;

} // namespace cxflow::testing
