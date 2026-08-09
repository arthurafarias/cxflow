// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include "CXORM/Core/SharedPointer.hpp"
#include <CXORM/Core/Containers/String.hpp>
#include <CXORM/Core/Containers/Map.hpp>

using namespace CXORM::Core::Containers;

namespace CXORM::Base {
using QueryResult = SharedPointer<Collection<SharedPointer<Map<String, String>>>>;
}