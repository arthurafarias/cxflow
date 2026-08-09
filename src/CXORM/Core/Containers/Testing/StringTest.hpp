// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <CXORM/Core/Containers/Collection.hpp>
#include <CXORM/Core/Containers/String.hpp>
#include <CXORM/Testing/TestGroup.hpp>

namespace CXORM::Core::Containers::Testing {

inline static ::CXORM::Testing::TestGroup StringTest{
    "String",
    {
        {"JoinOfEmptyCollectionIsEmpty",
         [](auto &ctx) {
           Collection<String> items;
           ctx.check_equal(String::join(items, ","), "");
         }},

        {"JoinOfSingleElementReturnsItUnchanged",
         [](auto &ctx) {
           Collection<String> items{String("only")};
           ctx.check_equal(String::join(items, ","), "only");
         }},

        {"JoinInsertsDelimiterBetweenEachPair",
         [](auto &ctx) {
           Collection<String> items{String("a"), String("b"), String("c")};
           ctx.check_equal(String::join(items, ", "), "a, b, c");
         }},

        {"TrimStripsControlCharactersLikeTabsAndNewlines",
         [](auto &ctx) { ctx.check_equal(String::trim("\t\nhello\r\n"), "hello"); }},

        {"TrimOnEmptyStringIsEmpty",
         [](auto &ctx) { ctx.check_equal(String::trim(""), ""); }},

        {"TrimOnStringWithNoLeadingOrTrailingJunkIsUnchanged",
         [](auto &ctx) { ctx.check_equal(String::trim("hello"), "hello"); }},

        // KNOWN GOTCHA: trim()/trim_left()/trim_right() keep a character as
        // soon as std::isprint() is true for it, and isprint(' ') is true -
        // space is a printable character, not a control character. So plain
        // ASCII spaces are NOT stripped, only genuine control characters
        // (tab, CR, LF, ...) are. A caller reading "trim" as "strip
        // whitespace" (the convention elsewhere, e.g. Python's str.strip())
        // will be surprised leading/trailing spaces survive.
        {"TrimDoesNotStripPlainSpaces_OnlyControlCharacters",
         [](auto &ctx) { ctx.check_equal(String::trim("  hello  "), "  hello  "); }},

        // KNOWN DEFECT, found while chasing this branch for coverage:
        // trim_right() on a string that is entirely non-printable does NOT
        // return "". Its underflow guard (`if (end == 0) break;`) exits the
        // loop with `end` still at 0 rather than decremented past it, and
        // the trailing `return source.substr(0, end + 1)` then keeps that
        // one byte at index 0 - regardless of the string's length. This is
        // masked in trim() (the entry point everyone actually calls): it
        // left-trims twice before right-trimming, and trim_left() has no
        // equivalent bug (its loop condition correctly runs past the end
        // and returns "" for an all-control-char input), so trim_right()
        // only ever sees an already-empty string there.
        {"TrimRightAloneLeavesOneCharacterOnAnAllControlCharacterString_KnownDefect",
         [](auto &ctx) {
           ctx.check_equal(String::trim_right(String("\x01\x02")), String("\x01"));
           ctx.check_equal(String::trim_right(String("\x01\x02\x03")), String("\x01"));
           ctx.check_equal(String::trim("\x01\x02"), "");
         }},

        // split() runs each piece through trim() before keeping it, so a
        // caller might reasonably expect surrounding spaces (a common CSV/
        // list-parsing need) to disappear. They don't, for the same reason
        // as the plain-spaces gotcha above.
        {"SplitOnSingleNeedleDoesNotStripSurroundingSpaces_KnownGotcha",
         [](auto &ctx) {
           auto parts = String::split("a, b,c ,  d", ",");
           if (!ctx.require_equal(parts.size(), 4u, "parts.size()")) {
             return;
           }
           ctx.check_equal(parts[0], "a");
           ctx.check_equal(parts[1], " b");
           ctx.check_equal(parts[2], "c ");
           ctx.check_equal(parts[3], "  d");
         }},

        {"SplitOnNeedleNotPresentReturnsWholeStringAsOneElement",
         [](auto &ctx) {
           auto parts = String::split("hello world", ";");
           if (!ctx.require_equal(parts.size(), 1u, "parts.size()")) {
             return;
           }
           ctx.check_equal(parts[0], "hello world");
         }},

        // KNOWN DEFECT: split(String, Collection<String> needles) is meant
        // to split on any of several delimiters (mirroring split(String,
        // String)), but its internal `find_any` helper returns as soon as
        // ONE needle is *absent* (`pos == npos`) rather than when one is
        // *found*. As long as every needle in the list DOES occur, the
        // for-loop runs to completion and falls through to its own
        // `return {-1, ""}`, which is itself treated as "not found" too -
        // so splitting "a,b;c" on {",", ";"} never yields ["a","b","c"].
        {"SplitByMultipleNeedlesNeverActuallySplits_KnownDefect",
         [](auto &ctx) {
           auto parts = String::split("a,b;c", Collection<String>{",", ";"});
           if (!ctx.require_equal(parts.size(), 1u, "parts.size()")) {
             return;
           }
           ctx.check_equal(parts[0], "a,b;c");
         }},

        // Same defect, reached via the OTHER branch of `find_any`: ";" is
        // genuinely absent here, so the early
        // `if (pos == npos) return {pos, needle};` fires directly on the
        // first needle that doesn't occur, before ever checking whether ","
        // (which IS present) would have been a valid split point.
        {"SplitByMultipleNeedlesBailsOnFirstAbsentNeedle_KnownDefect",
         [](auto &ctx) {
           auto parts = String::split("a,b,c", Collection<String>{",", ";"});
           if (!ctx.require_equal(parts.size(), 1u, "parts.size()")) {
             return;
           }
           ctx.check_equal(parts[0], "a,b,c");
         }},
    }};

} // namespace CXORM::Core::Containers::Testing
