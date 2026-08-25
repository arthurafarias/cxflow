## ---------------------------------------------------------------------------
## PROPRIETARY CODE – Arthur de Araújo Farias 2025
## All rights reserved.  No part of this file may be reproduced, stored in a
## retrieval system, or transmitted in any form or by any means—electronic,
## mechanical, photocopying, recording, or otherwise—without the prior written
## permission of the copyright holder.
## ---------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16)
project(media-streamer VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

include(CMakePackageConfigHelpers)

file(GLOB_RECURSE ${PROJECT_NAME}_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(${PROJECT_NAME} SHARED
  ${${PROJECT_NAME}_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)

target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Install header files
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
    FILES_MATCHING
        PATTERN "*.hpp"
)

# Export target file
install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION lib/cmake/${PROJECT_NAME}
)

# Generate and install the ConfigVersion file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Generate Config file manually (you can also write a .in template if needed)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeTemplates/Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION lib/cmake/${PROJECT_NAME}
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION lib/cmake/${PROJECT_NAME}
)

add_subdirectory(examples)

# Self-hosted test facility: no external test framework (gtest/catch2/etc),
# media-streamer stays dependency-free beyond the STL. Every tests/**/*.cpp
# (other than tests/framework/ itself) is compiled straight into one test
# binary; each declares a self-registering `test_group` at file scope (see
# tests/framework/test_group.hpp) so main.cpp needs no knowledge of what
# tests exist. CONFIGURE_DEPENDS means adding a new tests/**/*.cpp triggers
# CMake to pick it up on the next build.
option(MEDIA_STREAMER_BUILD_TESTS "Build media-streamer's self-hosted test facility" ON)
if(MEDIA_STREAMER_BUILD_TESTS)
  file(GLOB_RECURSE MEDIA_STREAMER_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  list(FILTER MEDIA_STREAMER_TEST_SOURCES EXCLUDE REGEX "/tests/framework/")

  add_executable(${PROJECT_NAME}-tests
    tests/framework/main.cpp
    ${MEDIA_STREAMER_TEST_SOURCES}
  )
  target_include_directories(${PROJECT_NAME}-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/framework)
  target_link_libraries(${PROJECT_NAME}-tests PRIVATE ${PROJECT_NAME})

  enable_testing()
  add_test(NAME ${PROJECT_NAME}-tests COMMAND ${PROJECT_NAME}-tests)
endif()
