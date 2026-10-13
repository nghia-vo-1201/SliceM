cmake_minimum_required(VERSION 3.21)

# Single source of truth for the version. A release tag must match it: scripts/release.ps1
# refuses to publish when the tag and this number disagree.
project(SliceM VERSION 0.2.0 LANGUAGES CXX)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/slicem_version.h.in
               ${CMAKE_BINARY_DIR}/generated/slicem_version.h @ONLY)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(SLICEM_BUILD_GUI "Build the Qt GUI" ON)

add_library(slicem_warnings INTERFACE)
if(MSVC)
  target_compile_options(slicem_warnings INTERFACE /W4)
else()
  target_compile_options(slicem_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

add_subdirectory(engine)
add_subdirectory(cli)
add_subdirectory(tests)

if(SLICEM_BUILD_GUI AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gui/CMakeLists.txt)
  find_package(Qt6 COMPONENTS Widgets OpenGLWidgets Network QUIET)
  if(Qt6_FOUND)
    add_subdirectory(gui)
  else()
    message(STATUS "Qt6 not found - skipping gui/ (engine, cli and tests still build)")
  endif()
endif()
