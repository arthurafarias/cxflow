// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cmath>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <variant>

namespace cxflow::containers {

// A closed-set typed value: the base storage unit for variant_map entries
// (see variant_map.hpp) and observable object properties (see object.hpp).
// No std::any/type-erasure escape hatch - matches structure.hpp's existing
// "no dynamic GValue-equivalent type system yet (deferred)" stance. Widen
// the base's scalar alternatives if/when a new field type is actually
// needed, rather than adding an open-ended fallback now.
//
// Closed set is bool/uint64_t/double_t/string plus the two self-referential
// aggregate alternatives below. Any argument not implicitly,
// non-narrowingly convertible to one of the six alternatives simply fails
// to compile at the call site, same as passing an unrelated type to a bare
// std::variant of these alternatives would.
//
// Publicly inherits std::variant instead of wrapping it, so the whole
// standard variant interface applies directly to this type: std::get,
// std::get_if, std::holds_alternative, std::visit, and operator== all work
// on a `variant` exactly as they would on a bare std::variant, found via
// template argument deduction through the (unique, public) base class and
// via ADL considering base classes - no bespoke get<T>()/holds<T>()
// re-implementation needed. `using base::base;` inherits std::variant's
// constructors as-is, including its converting constructor, which already
// excludes binding to another `variant` (so copy/move still go through the
// implicit special members, not the converting constructor) and already
// carries the C++20 fix (P0608) that makes a `const char*` prefer the
// std::string alternative over bool - without it, `variant v = "hello";`
// would silently pick the pre-P0608 boolean-conversion candidate instead
// of constructing a string.
//
// std::deque<variant> (an ordered list of variants) and
// std::map<std::string, variant> (a string-keyed, key-sorted collection of
// variants) make variant self-referential, with no pointer/indirection
// wrapper needed: std::deque tolerates an incomplete element type at the
// point it's instantiated (a guarantee deque/list/forward_list/vector
// specifically have - std::map is not guaranteed this by the standard),
// and in practice libstdc++'s and libc++'s std::map do not require the
// mapped type to be complete merely to be named as a variant alternative
// here either, since sizeof(map<K, V>) does not depend on sizeof(V).
// Verified to compile on GCC 16 and Clang 22 in C++23.
struct variant;
struct variant
    : public std::variant<bool, std::uint64_t, std::double_t, std::string,
                          std::deque<variant>, std::map<std::string, variant>> {
public:
  using base =
      std::variant<bool, std::uint64_t, std::double_t, std::string,
                   std::deque<variant>, std::map<std::string, variant>>;
  using base::variant;
};

} // namespace cxflow::containers
