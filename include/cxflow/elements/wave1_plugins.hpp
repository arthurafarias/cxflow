// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// SRS-004 §10.4's per-entry acceptance template requires every catalog
// entry to "register via plugin_registry (SRS-002 §5.4) under a sensible
// rank" - unlike SRS-002's own fake_src/identity/fake_sink trio (predating
// plugin_registry, where adopting it was left optional per REQ-5.2.3), this
// is a hard per-entry requirement for every new Wave 1 element. Each
// element keeps its own standalone register_type() (element_factory
// registration only, matching the rest of this codebase's convention and
// usable independently); this header additionally groups them into three
// plugins by category and registers each factory's rank/caps through
// plugin_registry - both paths are idempotent with each other (same type
// name, same underlying element_factory::register_type() call).
//
// All ranks are rank::primary: every entry here is the sole implementation
// of its type name, not one of several competing candidates a real
// autoplugger would need to rank against each other.

#include <cxflow/core/plugin_registry.hpp>
#include <cxflow/elements/app_sink.hpp>
#include <cxflow/elements/app_src.hpp>
#include <cxflow/elements/au_demux.hpp>
#include <cxflow/elements/au_mux.hpp>
#include <cxflow/elements/caps_filter.hpp>
#include <cxflow/elements/fd_sink.hpp>
#include <cxflow/elements/fd_src.hpp>
#include <cxflow/elements/file_sink.hpp>
#include <cxflow/elements/file_src.hpp>
#include <cxflow/elements/queue.hpp>
#include <cxflow/elements/tee.hpp>
#include <cxflow/elements/valve.hpp>
#include <cxflow/elements/wav_demux.hpp>
#include <cxflow/elements/wav_mux.hpp>

#include <memory>
#include <string>

namespace cxflow::elements {

inline static plugin_registration wave1_io_plugin_registration{
    plugin_info{"cxflow-io-elements", "File/descriptor/application sources and sinks (SRS-004 Wave 1)", "0.1.0",
                "Proprietary", "Arthur de Araújo Farias"},
    std::vector<element_factory_info>{
        element_factory_info{"file_src",
                              [](std::string name) { return std::make_shared<file_src>(std::move(name)); },
                              rank::primary, {}, {caps::any()}},
        element_factory_info{"file_sink",
                              [](std::string name) { return std::make_shared<file_sink>(std::move(name)); },
                              rank::primary, {caps::any()}, {}},
        element_factory_info{"fd_src", [](std::string name) { return std::make_shared<fd_src>(std::move(name)); },
                              rank::primary, {}, {caps::any()}},
        element_factory_info{"fd_sink", [](std::string name) { return std::make_shared<fd_sink>(std::move(name)); },
                              rank::primary, {caps::any()}, {}},
        element_factory_info{"app_src", [](std::string name) { return std::make_shared<app_src>(std::move(name)); },
                              rank::primary, {}, {caps::any()}},
        element_factory_info{"app_sink",
                              [](std::string name) { return std::make_shared<app_sink>(std::move(name)); },
                              rank::primary, {caps::any()}, {}},
    }};

inline static plugin_registration wave1_utility_plugin_registration{
    plugin_info{"cxflow-utility-elements", "Thread-decoupling, fan-out, gating, and caps-restriction elements "
                                            "(SRS-004 Wave 1)",
                "0.1.0", "Proprietary", "Arthur de Araújo Farias"},
    std::vector<element_factory_info>{
        element_factory_info{"queue", [](std::string name) { return std::make_shared<queue>(std::move(name)); },
                              rank::primary, {caps::any()}, {caps::any()}},
        element_factory_info{"tee", [](std::string name) { return std::make_shared<tee>(std::move(name)); },
                              rank::primary, {caps::any()}, {caps::any()}},
        element_factory_info{"valve", [](std::string name) { return std::make_shared<valve>(std::move(name)); },
                              rank::primary, {caps::any()}, {caps::any()}},
        element_factory_info{"caps_filter",
                              [](std::string name) { return std::make_shared<caps_filter>(std::move(name)); },
                              rank::primary, {caps::any()}, {caps::any()}},
    }};

inline static plugin_registration wave1_container_plugin_registration{
    plugin_info{"cxflow-container-elements", "RIFF/WAVE and Sun/NeXT AU linear-PCM container framing (SRS-004 "
                                              "Wave 1)",
                "0.1.0", "Proprietary", "Arthur de Araújo Farias"},
    std::vector<element_factory_info>{
        element_factory_info{"wav_mux", [](std::string name) { return std::make_shared<wav_mux>(std::move(name)); },
                              rank::primary, {caps::any()}, {caps::any()}},
        element_factory_info{"wav_demux",
                              [](std::string name) { return std::make_shared<wav_demux>(std::move(name)); },
                              rank::primary, {caps::any()}, {}},
        element_factory_info{"au_mux", [](std::string name) { return std::make_shared<au_mux>(std::move(name)); },
                              rank::primary, {caps::any()}, {caps::any()}},
        element_factory_info{"au_demux",
                              [](std::string name) { return std::make_shared<au_demux>(std::move(name)); },
                              rank::primary, {caps::any()}, {}},
    }};

} // namespace cxflow::elements
