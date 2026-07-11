cmake_minimum_required(VERSION 3.28)

project(libastro
  VERSION 0.0.1
  DESCRIPTION "Modern C++23 interface to the JPL DE440/DE441 ephemerides"
  LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

option(LIBASTRO_BUILD_TESTS "Build libastro tests" ${PROJECT_IS_TOP_LEVEL})
option(LIBASTRO_BUILD_EXAMPLES "Build libastro examples" ${PROJECT_IS_TOP_LEVEL})
option(LIBASTRO_BUILD_CLI "Build the astro command-line tool" ${PROJECT_IS_TOP_LEVEL})
option(LIBASTRO_INSTALL "Generate install and package (find_package) rules"
       ${PROJECT_IS_TOP_LEVEL})

# Vendored header-only argument parser (p-ranav/argparse, MIT) for the CLI.
add_library(argparse INTERFACE)
add_library(astro::argparse ALIAS argparse)
target_include_directories(argparse INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(mdspan)

add_library(astro
  src/ephemeris.cpp
  src/nutation.cpp
  src/phenomena.cpp
  src/reductions.cpp
  src/time.cpp
)
add_library(astro::astro ALIAS astro)

target_compile_features(astro PUBLIC cxx_std_23)
target_include_directories(astro PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
# mdspan is a build-only implementation detail of the record view: PRIVATE, and
# wrapped in $<BUILD_INTERFACE> so it is used only while building this library
# and never leaks into the exported/installed interface (no public header
# includes it, so consumers need nothing).
target_link_libraries(astro PRIVATE $<BUILD_INTERFACE:astro::mdspan>)
set_target_properties(astro PROPERTIES VERSION ${PROJECT_VERSION})

target_compile_options(astro PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)

if(LIBASTRO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

if(LIBASTRO_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(LIBASTRO_BUILD_CLI)
  add_subdirectory(cli)
endif()

# --- Install / packaging -----------------------------------------------------
# Makes the library consumable downstream via
#   find_package(astro CONFIG REQUIRED)
#   target_link_libraries(app PRIVATE astro::astro)
if(LIBASTRO_INSTALL)
  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)

  set(LIBASTRO_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/astro")

  install(TARGETS astro
    EXPORT astroTargets
    ARCHIVE  DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY  DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME  DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

  install(DIRECTORY include/astro
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

  install(EXPORT astroTargets
    NAMESPACE astro::
    FILE astroTargets.cmake
    DESTINATION "${LIBASTRO_CMAKEDIR}")

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/astroConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/astroConfig.cmake"
    INSTALL_DESTINATION "${LIBASTRO_CMAKEDIR}")

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/astroConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion)

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/astroConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/astroConfigVersion.cmake"
    DESTINATION "${LIBASTRO_CMAKEDIR}")
endif()
