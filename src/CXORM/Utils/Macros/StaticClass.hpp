// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <CXORM/Utils/Macros/DisableCopy.hpp>
#include <CXORM/Utils/Macros/DisableEmptyConstructor.hpp>
#include <CXORM/Utils/Macros/DisableMove.hpp>

#define StaticClass(ClassName)                                                 \
  DisableEmptyConstructor(ClassName) DisableCopy(ClassName)                    \
      DisableMove(ClassName)
