## ---------------------------------------------------------------------------
## PROPRIETARY CODE – Arthur de Araújo Farias 2025
## All rights reserved.  No part of this file may be reproduced, stored in a
## retrieval system, or transmitted in any form or by any means—electronic,
## mechanical, photocopying, recording, or otherwise—without the prior written
## permission of the copyright holder.
## ---------------------------------------------------------------------------

add_executable(cxflow-launch cxflow-launch/main.cpp)
target_link_libraries(cxflow-launch PRIVATE cxflow)

# Optional plugins (Plugin Folder Convention REQ-P.5): link and register
# each one only if its own CMakeLists.txt actually produced a target for
# it (i.e. the plugin's dependency was found and CXFLOW_PLUGIN_<NAME> is
# on) - cxflow-launch stays buildable with zero plugins enabled.
if(TARGET cxflow-plugin-pulse)
  target_link_libraries(cxflow-launch PRIVATE cxflow-plugin-pulse)
  target_compile_definitions(cxflow-launch PRIVATE CXFLOW_PLUGIN_PULSE_ENABLED)
endif()
