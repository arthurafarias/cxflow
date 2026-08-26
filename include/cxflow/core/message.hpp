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

namespace cxflow {

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

inline const char *to_string(message_type t) {
  switch (t) {
  case message_type::state_changed:
    return "state_changed";
  case message_type::eos:
    return "eos";
  case message_type::error:
    return "error";
  case message_type::warning:
    return "warning";
  case message_type::info:
    return "info";
  case message_type::buffering:
    return "buffering";
  }
  return "unknown";
}

} // namespace cxflow
