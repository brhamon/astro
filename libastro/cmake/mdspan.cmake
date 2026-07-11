# Resolve a std::mdspan implementation, in priority order:
#   1. A native <mdspan> from the standard library (future toolchains).
#   2. A vendored Kokkos reference impl under third_party/mdspan.
#   3. FetchContent from GitHub (requires network at configure time).
#
# Produces an INTERFACE target `astro::mdspan` that carries the right include
# path (and, for the native case, nothing at all).

include_guard(GLOBAL)

add_library(astro_mdspan INTERFACE)
add_library(astro::mdspan ALIAS astro_mdspan)

include(CheckIncludeFileCXX)
set(CMAKE_REQUIRED_FLAGS "-std=c++23")
check_include_file_cxx("mdspan" LIBASTRO_HAVE_NATIVE_MDSPAN)
unset(CMAKE_REQUIRED_FLAGS)

if(LIBASTRO_HAVE_NATIVE_MDSPAN)
  message(STATUS "libastro: using native <mdspan>")
  target_compile_definitions(astro_mdspan INTERFACE LIBASTRO_NATIVE_MDSPAN=1)
  return()
endif()

set(_vendored "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mdspan")
if(EXISTS "${_vendored}/CMakeLists.txt")
  message(STATUS "libastro: using vendored mdspan at ${_vendored}")
  add_subdirectory("${_vendored}" "${CMAKE_BINARY_DIR}/_mdspan" EXCLUDE_FROM_ALL)
  target_link_libraries(astro_mdspan INTERFACE mdspan)
  return()
endif()

message(STATUS "libastro: mdspan not vendored; falling back to FetchContent "
               "(run scripts/vendor_mdspan.sh to avoid network at configure)")
include(FetchContent)
FetchContent_Declare(
  mdspan
  GIT_REPOSITORY https://github.com/kokkos/mdspan.git
  GIT_TAG stable
  GIT_SHALLOW TRUE
  # EXCLUDE_FROM_ALL keeps mdspan's own install() rules out of our install tree:
  # it is a build-only private dependency (see the BUILD_INTERFACE link below).
  EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(mdspan)
target_link_libraries(astro_mdspan INTERFACE mdspan)
