// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <cxflow/core/caps.hpp>
#include <cxflow/core/element_factory.hpp>

namespace cxflow {

// SRS-002 §5.1: fixed at registration time and never mutated/observed
// afterward (REQ-5.1.2) - plain data, not containers::object, unlike caps/
// structure (SRS-001) which are walked/observed live.
struct plugin_info {
  std::string name; // e.g. "cxflow-fake-elements"
  std::string description;
  std::string version; // semver string, project's own versioning
  std::string license;  // SPDX identifier, e.g. "Proprietary" or "MIT"
  std::string author;
};

// SRS-002 §7.2: values match GST_RANK_* numerically, not just in relative
// order, so a rank ported from a real GStreamer plugin (SRS-004) can be
// copied across unchanged.
enum class rank : std::int32_t { none = 0, marginal = 64, secondary = 128, primary = 256 };

// SRS-002 §5.3: per-factory metadata and rank, declared at registration time
// (REQ-5.3.2) rather than discovered by instantiating the element, so §5.5's
// caps-based lookup can scan without constructing every candidate.
struct element_factory_info {
  std::string type_name; // registry key, e.g. "fake_src"
  element_factory::creator_function creator;
  rank factory_rank = rank::none;
  std::vector<caps> input_caps;  // empty = no sink pad / accepts nothing
  std::vector<caps> output_caps; // empty = no src pad / produces nothing
};

} // namespace cxflow
