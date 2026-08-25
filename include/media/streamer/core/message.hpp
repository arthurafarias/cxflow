// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <exception>
#include <memory>
#include <string>

namespace media::streamer {

class element;

enum class message_type {
  state_changed,
  eos,
  error,
  warning,
  info,
  buffering,
};

struct message {
  message_type type;
  std::weak_ptr<element> source;
  std::string debug_info;
  std::exception_ptr error; // populated for message_type::error/warning
};

} // namespace media::streamer
