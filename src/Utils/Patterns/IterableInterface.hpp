// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

template <typename Derived> class IterableInterface : public Core::Object {
public:
  virtual SharedPointer<Derived> next(int offset) = 0;
  virtual SharedPointer<Derived> previous(int offset) = 0;
};