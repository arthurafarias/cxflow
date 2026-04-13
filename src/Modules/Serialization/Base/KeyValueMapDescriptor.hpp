// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include "Core/Containers/Collection.hpp"
#include "Core/Containers/Variant.hpp"

using namespace CXORM::Core::Containers;

namespace CXORM::Serialization::Base {
template <typename... ValueTypes>
struct KeyValueMapDescriptor : Collection<Variant<ValueTypes...>> {};
} // namespace Serialization