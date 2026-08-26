// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include <cxflow/containers/variant.hpp>
#include <cxflow/core/bin.hpp>
#include <cxflow/core/bus.hpp>
#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/pad.hpp>

namespace cxflow {

// A bin that owns a bus for asynchronous message delivery to the
// application driving it. bin::add() propagates this bus down to every
// child added, which is how fake_sink (and any future element) reaches a
// bus to post to without holding a parent pointer. Full clock/base-time
// synchronized playback is deferred - pipeline holds no clock in this pass.
class pipeline : public bin {
public:
  explicit pipeline(std::string name) : bin(std::move(name)) { set_bus(std::make_shared<class bus>()); }

  // Always non-null: the constructor above guarantees it. Hides (not
  // overrides - non-virtual) element::bus(), which returns a nullable
  // shared_ptr for elements that may not belong to a pipeline yet.
  class bus &bus() { return *element::bus(); }

  // SRS-003 §5.3/§5.5: a point-in-time snapshot (REQ-5.5.2 - each child's
  // properties come from object::begin()'s own snapshot iterator, so this
  // never blocks the data plane) of this pipeline's shape as one
  // containers::variant tree - see §5.3 for the exact map/list shape.
  // Nested bins (REQ-5.3.5) are not implemented in this pass: no bin
  // subclass other than pipeline itself is ever a bin's child in this
  // codebase yet, and §9's acceptance only requires a flat, linked,
  // multi-element pipeline to round-trip.
  containers::variant to_variant() const;

  // REQ-5.3.4: creates every child via element_factory::create() by the
  // "type" field - the exact same creation primitive §5.2's text-grammar
  // parser uses, so a pipeline built from either description form is
  // indistinguishable afterward. Trusts description to already be
  // well-formed (normally this method's own to_variant() output) - unlike
  // pipeline_parser::parse() (§5.2), there is no user-facing text boundary
  // here to report a position/message error against, so a malformed tree
  // throws std::bad_variant_access/std::out_of_range same as misusing any
  // other internal, precondition-bearing API.
  static std::shared_ptr<pipeline> from_variant(const containers::variant &description);
};

inline containers::variant pipeline::to_variant() const {
  using containers::variant;

  std::map<std::string, variant> root;
  root.emplace("name", variant(name()));

  std::deque<variant> children_v;
  for (const auto &child : children()) {
    std::map<std::string, variant> child_v;
    child_v.emplace("type", variant(child->registered_type_name()));
    child_v.emplace("name", variant(child->name()));

    // REQ-5.3.1: object::begin()/end()'s snapshot walk, no per-element-type
    // serialization code.
    std::map<std::string, variant> properties_v;
    for (const auto &[key, value] : *child) {
      properties_v.emplace(key, value);
    }
    child_v.emplace("properties", variant(std::move(properties_v)));

    // REQ-5.3.2: derived from pad::is_linked()/peer() walked once per
    // element - only src-side links are recorded, so each link appears
    // exactly once across the whole pipeline (its sink-side peer is
    // reconstructed by from_variant() below).
    std::deque<variant> links_v;
    for (const auto &p : child->pads()) {
      if (p->dir() != pad::direction::src || !p->is_linked()) {
        continue;
      }
      std::map<std::string, variant> link_v;
      link_v.emplace("src-pad", variant(p->name()));
      link_v.emplace("to", variant(p->peer()->owner().name()));
      link_v.emplace("sink-pad", variant(p->peer()->name()));
      links_v.push_back(variant(std::move(link_v)));
    }
    child_v.emplace("links", variant(std::move(links_v)));

    children_v.push_back(variant(std::move(child_v)));
  }
  root.emplace("children", variant(std::move(children_v)));

  return variant(std::move(root));
}

inline std::shared_ptr<pipeline> pipeline::from_variant(const containers::variant &description) {
  using containers::variant;

  const auto &root = std::get<std::map<std::string, variant>>(description);
  auto pipe = std::make_shared<pipeline>(std::get<std::string>(root.at("name")));

  if (!root.contains("children")) {
    return pipe;
  }
  const auto &children_v = std::get<std::deque<variant>>(root.at("children"));

  // Pass 1: create every element and set its properties, before any
  // linking - REQ-5.3.4's creation primitive, mirroring §5.2's parser.
  // Two passes because a link can name a peer that appears later in
  // "children" (to_variant() only records src-side links, so the *last*
  // element in a chain is only ever referenced as a link target, never as
  // the one iterated to emit a link).
  std::map<std::string, std::shared_ptr<element>> by_name;
  for (const auto &child_v : children_v) {
    const auto &child_map = std::get<std::map<std::string, variant>>(child_v);
    std::string type = std::get<std::string>(child_map.at("type"));
    std::string instance_name = std::get<std::string>(child_map.at("name"));

    auto el = element_factory::create(type, instance_name);
    if (!el) {
      continue; // unknown type - see class comment: description is trusted, not user-facing text
    }
    pipe->add(el);
    by_name.emplace(instance_name, el);

    if (child_map.contains("properties")) {
      const auto &properties_v = std::get<std::map<std::string, variant>>(child_map.at("properties"));
      for (const auto &[key, value] : properties_v) {
        el->property_set(key, value);
      }
    }
  }

  // Pass 2: links, now that every element named by a "to" field exists.
  for (const auto &child_v : children_v) {
    const auto &child_map = std::get<std::map<std::string, variant>>(child_v);
    auto from_it = by_name.find(std::get<std::string>(child_map.at("name")));
    if (from_it == by_name.end() || !child_map.contains("links")) {
      continue;
    }

    const auto &links_v = std::get<std::deque<variant>>(child_map.at("links"));
    for (const auto &link_v : links_v) {
      const auto &link_map = std::get<std::map<std::string, variant>>(link_v);
      auto to_it = by_name.find(std::get<std::string>(link_map.at("to")));
      if (to_it == by_name.end()) {
        continue;
      }

      pad *src_pad = from_it->second->get_static_pad(std::get<std::string>(link_map.at("src-pad")));
      pad *sink_pad = to_it->second->get_static_pad(std::get<std::string>(link_map.at("sink-pad")));
      if (src_pad != nullptr && sink_pad != nullptr) {
        src_pad->link(*sink_pad);
      }
    }
  }

  return pipe;
}

} // namespace cxflow
