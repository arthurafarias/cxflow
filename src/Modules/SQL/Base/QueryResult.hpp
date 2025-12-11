// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <Core/Containers/String.hpp>
#include <Core/Containers/Map.hpp>

using namespace Core::Containers;

namespace Modules::SQL::SQLite
{
using QueryResult = Collection<SharedPointer<Map<String, String>>>;
}