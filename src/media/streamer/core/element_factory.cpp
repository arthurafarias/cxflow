// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#include <media/streamer/core/element_factory.hpp>

#include <map>
#include <mutex>

namespace media::streamer {

namespace {

struct registry {
  std::mutex mutex;
  std::map<std::string, element_factory::creator_function> creators;

  static registry &instance() {
    static registry r;
    return r;
  }
};

} // namespace

void element_factory::register_type(std::string type_name, creator_function creator) {
  registry &r = registry::instance();
  std::unique_lock lock(r.mutex);
  r.creators[std::move(type_name)] = std::move(creator);
}

std::shared_ptr<element> element_factory::create(const std::string &type_name, std::string instance_name) {
  registry &r = registry::instance();
  std::unique_lock lock(r.mutex);

  auto it = r.creators.find(type_name);
  if (it == r.creators.end()) {
    return nullptr;
  }

  auto creator = it->second;
  lock.unlock();

  return creator(std::move(instance_name));
}

} // namespace media::streamer
