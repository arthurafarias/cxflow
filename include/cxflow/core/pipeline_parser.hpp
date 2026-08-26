// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <vector>

#include <cxflow/containers/variant.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/pad.hpp>
#include <cxflow/core/pipeline.hpp>

namespace cxflow {

// SRS-003 §5.1/§5.2: `element (link element)*` (link := "!"), gst-launch-
// shaped. Single-pass, left-to-right (REQ-5.2.1) - every element is created
// and added to the returned pipeline via bin::add() before parsing
// continues (REQ-5.2.2), and a parse failure reports where it happened
// rather than throwing or returning a silent null (REQ-5.2.3).
class pipeline_parser {
public:
  struct error {
    std::string message;
    std::size_t position; // byte offset into the source description
  };

  static std::expected<std::shared_ptr<pipeline>, error> parse(const std::string &description);
};

namespace detail {

struct pipeline_parser_token {
  std::string text;
  std::size_t position;
};

// Whitespace-delimited tokens only (REQ-5.1.5's "linear/branching case
// only" scope) - no quoting yet (§8 OPEN-D3, deliberately not designed
// speculatively ahead of real usage).
inline std::vector<pipeline_parser_token> pipeline_parser_tokenize(const std::string &description) {
  std::vector<pipeline_parser_token> tokens;
  std::size_t i = 0;
  while (i < description.size()) {
    while (i < description.size() && std::isspace(static_cast<unsigned char>(description[i]))) {
      ++i;
    }
    if (i >= description.size()) {
      break;
    }
    std::size_t start = i;
    while (i < description.size() && !std::isspace(static_cast<unsigned char>(description[i]))) {
      ++i;
    }
    tokens.push_back(pipeline_parser_token{description.substr(start, i - start), start});
  }
  return tokens;
}

// REQ-5.1.2's type-inference order, with one deliberate deviation from its
// literal wording: signed std::int64_t is tried before unsigned
// std::uint64_t, not instead of it. A parser with no per-element-type
// knowledge (REQ-5.1.2's own "no bespoke per-element-type property-setting
// code path") has to pick one common integer type, and fake_src's own
// "num-buffers" - this SRS's §9 acceptance example - is int64_t
// specifically so -1 keeps its "unbounded" sentinel meaning (see
// fake_src.hpp). Defaulting to uint64_t would make property_set() store an
// alternative element::property_get<std::int64_t>() cannot std::get<>() out
// of, throwing std::bad_variant_access the moment fake_src reads it back -
// i.e. the acceptance example itself would crash. int64_t covers every
// practical signed/unsigned value up to 2^63-1; uint64_t remains the
// fallback for the rare magnitude beyond that.
inline containers::variant pipeline_parser_value(const std::string &s) {
  if (s == "true") {
    return containers::variant(true);
  }
  if (s == "false") {
    return containers::variant(false);
  }

  std::int64_t i64 = 0;
  auto [iptr, iec] = std::from_chars(s.data(), s.data() + s.size(), i64);
  if (iec == std::errc{} && iptr == s.data() + s.size()) {
    return containers::variant(i64);
  }

  std::uint64_t u64 = 0;
  auto [uptr, uec] = std::from_chars(s.data(), s.data() + s.size(), u64);
  if (uec == std::errc{} && uptr == s.data() + s.size()) {
    return containers::variant(u64);
  }

  double d = 0;
  auto [dptr, dec] = std::from_chars(s.data(), s.data() + s.size(), d);
  if (dec == std::errc{} && dptr == s.data() + s.size()) {
    return containers::variant(static_cast<std::double_t>(d));
  }

  return containers::variant(s);
}

// REQ-5.1.3: the "next unlinked" pad of the given direction - the first one
// with no peer yet - or nullptr if every pad of that direction (or none
// exist) is already linked.
inline pad *pipeline_parser_first_unlinked(element &el, pad::direction dir) {
  for (const auto &p : el.pads()) {
    if (p->dir() == dir && !p->is_linked()) {
      return p.get();
    }
  }
  return nullptr;
}

} // namespace detail

inline std::expected<std::shared_ptr<pipeline>, pipeline_parser::error>
pipeline_parser::parse(const std::string &description) {
  auto tokens = detail::pipeline_parser_tokenize(description);
  if (tokens.empty()) {
    return std::unexpected(error{"empty pipeline description", 0});
  }

  auto pipe = std::make_shared<pipeline>("pipeline");

  std::shared_ptr<element> pending_link_from; // non-null right after a '!', linked once the next element exists
  std::size_t element_index = 0;
  std::size_t i = 0;

  while (i < tokens.size()) {
    const detail::pipeline_parser_token &type_token = tokens[i];
    if (type_token.text == "!") {
      return std::unexpected(error{"expected an element type name, got '!'", type_token.position});
    }
    ++i;

    // REQ-5.1.4: name-assignment is optional and, per the grammar, can only
    // appear directly after the type name - it must be resolved before
    // element_factory::create() below, since it supplies that call's
    // instance name.
    std::string instance_name;
    if (i < tokens.size() && tokens[i].text.starts_with("name=")) {
      instance_name = tokens[i].text.substr(5);
      ++i;
    } else {
      instance_name = type_token.text + "-" + std::to_string(element_index);
    }
    ++element_index;

    auto el = element_factory::create(type_token.text, instance_name);
    if (!el) {
      return std::unexpected(error{"unknown element type '" + type_token.text + "'", type_token.position});
    }
    pipe->add(el); // REQ-5.2.2: added immediately, before any later token can fail to parse

    if (pending_link_from) {
      pad *src_pad = detail::pipeline_parser_first_unlinked(*pending_link_from, pad::direction::src);
      pad *sink_pad = detail::pipeline_parser_first_unlinked(*el, pad::direction::sink);

      if (sink_pad == nullptr) {
        return std::unexpected(
            error{"cannot link '" + pending_link_from->name() + "' to '" + el->name() + "'", type_token.position});
      }

      if (src_pad != nullptr) {
        if (!src_pad->link(*sink_pad)) {
          return std::unexpected(
              error{"cannot link '" + pending_link_from->name() + "' to '" + el->name() + "'", type_token.position});
        }
      } else {
        // SRS-004 §8 OPEN-M2: the upstream element has no src pad yet (a
        // demuxer that only knows its output format once it has parsed
        // enough of the stream, e.g. wav_demux/au_demux) - defer the link
        // to the moment element::pad_added fires for it, exactly the way
        // gst_parse_launch itself handles a dynamic-pad element on the left
        // of '!'. A link that fails once the pad exists (incompatible
        // caps) is reported the same way any other runtime pad::link()
        // failure already is - a warn-level journal entry from pad::link()
        // itself - since this function has long since returned by then and
        // has no error channel left to report through.
        pending_link_from->pad_added.connect([sink_pad](element &, pad &new_pad) {
          if (new_pad.dir() == pad::direction::src && !new_pad.is_linked() && !sink_pad->is_linked()) {
            new_pad.link(*sink_pad);
          }
        });
      }

      pending_link_from.reset();
    }

    while (i < tokens.size() && tokens[i].text != "!") {
      const detail::pipeline_parser_token &prop_token = tokens[i];
      auto eq = prop_token.text.find('=');
      if (eq == std::string::npos) {
        return std::unexpected(
            error{"expected 'key=value' or '!', got '" + prop_token.text + "'", prop_token.position});
      }
      std::string key = prop_token.text.substr(0, eq);
      std::string value_text = prop_token.text.substr(eq + 1);
      el->property_set(key, detail::pipeline_parser_value(value_text));
      ++i;
    }

    if (i < tokens.size() && tokens[i].text == "!") {
      std::size_t link_position = tokens[i].position;
      ++i;
      if (i >= tokens.size()) {
        return std::unexpected(error{"expected an element after '!'", link_position});
      }
      pending_link_from = el;
    }
  }

  return pipe;
}

} // namespace cxflow
