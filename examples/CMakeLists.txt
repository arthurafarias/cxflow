## ---------------------------------------------------------------------------
## PROPRIETARY CODE – Arthur de Araújo Farias 2025
## All rights reserved.  No part of this file may be reproduced, stored in a
## retrieval system, or transmitted in any form or by any means—electronic,
## mechanical, photocopying, recording, or otherwise—without the prior written
## permission of the copyright holder.
## ---------------------------------------------------------------------------

add_executable(${PROJECT_NAME}-examples-person person.cpp)
target_link_libraries(${PROJECT_NAME}-examples-person PUBLIC cxorm)

add_executable(${PROJECT_NAME}-examples-person-direct-mapping person-direct-mapping.cpp)
target_link_libraries(${PROJECT_NAME}-examples-person-direct-mapping PUBLIC cxorm)

if(CXORM_HAVE_MARIADB)
  add_executable(${PROJECT_NAME}-examples-mariadb-person mariadb-person.cpp)
  target_link_libraries(${PROJECT_NAME}-examples-mariadb-person PUBLIC cxorm)

  add_executable(${PROJECT_NAME}-examples-mariadb-person-direct-mapping
    mariadb-person-direct-mapping.cpp)
  target_link_libraries(${PROJECT_NAME}-examples-mariadb-person-direct-mapping PUBLIC cxorm)
endif()