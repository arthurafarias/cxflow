// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/pipeline_parser.hpp>
#include <cxflow/core/plugin_registry.hpp>
#include <cxflow/elements/fake_sink.hpp>
#include <cxflow/elements/fake_src.hpp>
#include <cxflow/elements/identity.hpp>
#include <cxflow/elements/wav_demux.hpp>
#include <cxflow/testing/test_group.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace cxflow::testing {

// A property-only test type - no pads - so property-type-inference cases
// (below) don't also need a linkable chain around them.
inline static plugin_registration pipeline_parser_test_props_type_registration{
    plugin_info{"pipeline_parser_test.props_plugin", "d", "1.0.0", "MIT", "test"},
    std::vector<element_factory_info>{element_factory_info{
        "pipeline_parser_test.props", [](std::string name) { return std::make_shared<element>(std::move(name)); }}}};

struct pipeline_parser_test : public test_group {
  pipeline_parser_test() : test_group("pipeline_parser", {
    {"parses a linear chain, linking each element's next unlinked pads", [](test_context &ctx) {
      elements::fake_src::register_type();
      elements::identity::register_type();
      elements::fake_sink::register_type();

      auto result = pipeline_parser::parse("fake_src ! identity ! fake_sink");
      ctx.require(result.has_value(), "a well-formed description should parse successfully");

      auto pipe = *result;
      ctx.require_equal(pipe->children().size(), std::size_t{3});

      pad *src_out = pipe->children()[0]->get_static_pad("src");
      pad *id_in = pipe->children()[1]->get_static_pad("sink");
      pad *id_out = pipe->children()[1]->get_static_pad("src");
      pad *sink_in = pipe->children()[2]->get_static_pad("sink");

      ctx.require(src_out != nullptr && id_in != nullptr && id_out != nullptr && sink_in != nullptr,
                  "every element in the chain should expose the expected pad");
      ctx.check(src_out->peer() == id_in, "fake_src's src pad should link to identity's sink pad");
      ctx.check(id_out->peer() == sink_in, "identity's src pad should link to fake_sink's sink pad");
    }},
    {"name= assigns the instance name", [](test_context &ctx) {
      elements::fake_src::register_type();
      elements::fake_sink::register_type();

      auto result = pipeline_parser::parse("fake_src name=my-source ! fake_sink name=my-sink");
      ctx.require(result.has_value(), "a well-formed description should parse successfully");

      auto pipe = *result;
      ctx.check_equal(pipe->children()[0]->name(), std::string("my-source"));
      ctx.check_equal(pipe->children()[1]->name(), std::string("my-sink"));
    }},
    {"an unnamed element gets an implementation-defined unique name", [](test_context &ctx) {
      elements::fake_src::register_type();
      elements::fake_sink::register_type();

      auto result = pipeline_parser::parse("fake_src ! fake_sink");
      ctx.require(result.has_value(), "a well-formed description should parse successfully");

      auto pipe = *result;
      ctx.check(!pipe->children()[0]->name().empty(), "an unnamed element should still get a non-empty name");
      ctx.check(!pipe->children()[1]->name().empty(), "an unnamed element should still get a non-empty name");
      ctx.check(pipe->children()[0]->name() != pipe->children()[1]->name(),
                 "two unnamed elements should get distinct names");
    }},
    {"property values are type-inferred: bool/int64_t/uint64_t/double/string", [](test_context &ctx) {
      auto result = pipeline_parser::parse(
          "pipeline_parser_test.props flag=true count=42 big=18000000000000000000 ratio=3.14 label=hello");
      ctx.require(result.has_value(), "a well-formed description should parse successfully");

      auto &el = *(*result)->children()[0];
      ctx.check_equal(el.property_get<bool>("flag").value_or(false), true);
      ctx.check_equal(el.property_get<std::int64_t>("count").value_or(0), std::int64_t{42});
      ctx.check_equal(el.property_get<std::uint64_t>("big").value_or(0), std::uint64_t{18000000000000000000ULL});
      ctx.check_equal(el.property_get<double>("ratio").value_or(0.0), 3.14);
      ctx.check_equal(el.property_get<std::string>("label").value_or(""), std::string("hello"));
    }},
    {"an integer literal parses as int64_t, matching fake_src's own property type", [](test_context &ctx) {
      // Regression case for the deviation documented in pipeline_parser.hpp:
      // REQ-5.1.2's literal wording (always uint64_t) would make this exact
      // SRS-003 §9 acceptance command throw std::bad_variant_access, since
      // fake_src reads "num-buffers" back via property_get<std::int64_t>().
      elements::fake_src::register_type();
      elements::identity::register_type();
      elements::fake_sink::register_type();

      auto result = pipeline_parser::parse("fake_src num-buffers=3 ! identity ! fake_sink");
      ctx.require(result.has_value(), "a well-formed description should parse successfully");

      auto &src = *(*result)->children()[0];
      ctx.check_equal(src.property_get<std::int64_t>("num-buffers").value_or(-99), std::int64_t{3});
    }},
    {"defers linking to a demuxer's dynamically-added src pad (SRS-004 §8 OPEN-M2)", [](test_context &ctx) {
      elements::wav_demux::register_type();
      elements::fake_sink::register_type();

      auto result = pipeline_parser::parse("wav_demux ! fake_sink");
      ctx.require(result.has_value(), "linking to an element with no src pad yet should not be a parse error");

      auto pipe = *result;
      element &demux = *pipe->children()[0];
      element &sink = *pipe->children()[1];

      pad &new_src = demux.add_pad(std::make_unique<pad>("src", pad::direction::src, demux));
      ctx.check(new_src.is_linked(), "the dynamically-added pad should link once it appears");
      ctx.check(new_src.peer() == sink.get_static_pad("sink"), "it should link specifically to the downstream sink pad");
    }},
    {"an unknown element type is a parse error naming the offending token", [](test_context &ctx) {
      auto result = pipeline_parser::parse("pipeline_parser_test.does_not_exist");
      ctx.require(!result.has_value(), "an unregistered type name should fail to parse");
      ctx.check(result.error().message.find("pipeline_parser_test.does_not_exist") != std::string::npos,
                 "the error message should name the offending type");
      ctx.check_equal(result.error().position, std::size_t{0});
    }},
    {"a link with no free pad on either side is a parse error naming both elements", [](test_context &ctx) {
      elements::fake_src::register_type();
      elements::fake_sink::register_type();

      // fake_sink has no src pad to link onward from.
      auto result = pipeline_parser::parse("fake_sink ! fake_src");
      ctx.require(!result.has_value(), "linking from a pad-less side should fail to parse");
      ctx.check(result.error().message.find("fake_sink") != std::string::npos &&
                     result.error().message.find("fake_src") != std::string::npos,
                 "the error message should name both elements involved in the failed link");
    }},
    {"an empty description is a parse error", [](test_context &ctx) {
      auto result = pipeline_parser::parse("");
      ctx.require(!result.has_value(), "an empty description should fail to parse");
      ctx.check_equal(result.error().position, std::size_t{0});
    }},
    {"a leading '!' is a parse error", [](test_context &ctx) {
      elements::fake_src::register_type();
      auto result = pipeline_parser::parse("! fake_src");
      ctx.require(!result.has_value(), "a description starting with '!' should fail to parse");
      ctx.check_equal(result.error().position, std::size_t{0});
    }},
    {"a trailing '!' with nothing after it is a parse error", [](test_context &ctx) {
      elements::fake_src::register_type();
      auto result = pipeline_parser::parse("fake_src !");
      ctx.require(!result.has_value(), "a description ending with '!' should fail to parse");
    }},
    {"a token that is neither 'key=value' nor '!' is a parse error", [](test_context &ctx) {
      elements::fake_src::register_type();
      auto result = pipeline_parser::parse("fake_src not-a-property");
      ctx.require(!result.has_value(), "a bare token with no '=' should fail to parse");
    }},
  }) {}
};

inline static pipeline_parser_test pipeline_parser_test_instance;

} // namespace cxflow::testing
