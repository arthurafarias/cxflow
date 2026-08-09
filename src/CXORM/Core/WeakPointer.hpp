// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <memory>

template<typename ...ArgsTypes>
class WeakPointer : public std::weak_ptr<ArgsTypes...>
{
    public:
    using std::weak_ptr<ArgsTypes...>::weak_ptr;
};