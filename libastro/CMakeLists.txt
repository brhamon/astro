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
# mdspan is an implementation detail of the record view -> PRIVATE.
target_link_libraries(astro PRIVATE astro::mdspan)

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
