// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <functional>
#include <memory>
#include <string>

#include <media/streamer/core/element.hpp>

namespace media::streamer {

// Static in-process name -> creator registry. Later modules register their
// own element types here without the core knowing about them upfront -
// deliberately lighter than GStreamer's plugin/.so registry, since dynamic
// loading is out of scope for this pass.
class element_factory {
public:
  using creator_function = std::function<std::shared_ptr<element>(std::string name)>;

  static void register_type(std::string type_name, creator_function creator);
  static std::shared_ptr<element> create(const std::string &type_name, std::string instance_name);
};

} // namespace media::streamer
