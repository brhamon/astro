# Ephemeris fetched via scripts/fetch_ephemeris.sh; the vector/constants tests
# need it present.
set(_jpleph "${CMAKE_CURRENT_SOURCE_DIR}/../data/JPLEPH")

# Oracle outputs are generated per *selected* ephemeris into the build tree and
# never committed: they are large and ephemeris-specific, and JPL revises the DE
# series periodically. The generators (small) are checked in; CTest fixtures
# below run them before the tests that consume them.
set(_state_csv "${CMAKE_CURRENT_BINARY_DIR}/state_vectors.csv")
set(_const_csv "${CMAKE_CURRENT_BINARY_DIR}/constants_ref.csv")
set(_place_csv "${CMAKE_CURRENT_BINARY_DIR}/place.csv")
set(_nut_csv "${CMAKE_CURRENT_BINARY_DIR}/nutation.csv")
set(_hor_csv "${CMAKE_CURRENT_BINARY_DIR}/hor.csv")
set(_time_csv "${CMAKE_CURRENT_BINARY_DIR}/time.csv")

# --- Generators (oracles) ----------------------------------------------------
# gen_vectors links the legacy NOVAS-C archive from the parent repo.
set(_novas_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../novasc3.1")
if(EXISTS "${_novas_dir}/novas.a")
  enable_language(C)
  add_executable(gen_vectors gen/gen_vectors.c)
  target_include_directories(gen_vectors PRIVATE "${_novas_dir}")
  target_link_libraries(gen_vectors PRIVATE "${_novas_dir}/novas.a" m)
  add_executable(gen_place gen/gen_place.c)
  target_include_directories(gen_place PRIVATE "${_novas_dir}")
  target_link_libraries(gen_place PRIVATE "${_novas_dir}/novas.a" m)
  add_executable(gen_nutation gen/gen_nutation.c)
  target_include_directories(gen_nutation PRIVATE "${_novas_dir}")
  target_link_libraries(gen_nutation PRIVATE "${_novas_dir}/novas.a" m)
  add_executable(gen_hor gen/gen_hor.c)
  target_include_directories(gen_hor PRIVATE "${_novas_dir}")
  target_link_libraries(gen_hor PRIVATE "${_novas_dir}/novas.a" m)
  add_executable(gen_time gen/gen_time.c)
  target_include_directories(gen_time PRIVATE "${_novas_dir}")
  target_link_libraries(gen_time PRIVATE "${_novas_dir}/novas.a" m)
  message(STATUS "libastro: NOVAS oracle found; gen_vectors/gen_place/"
                 "gen_nutation/gen_hor/gen_time enabled")
else()
  message(STATUS "libastro: NOVAS not built (run `make` in the parent repo); "
                 "gen_vectors disabled -> replay test will skip")
endif()

# gen_constants reuses con440.c (repo root); no NOVAS needed.
set(_con440 "${CMAKE_CURRENT_SOURCE_DIR}/../../con440.c")
if(EXISTS "${_con440}")
  enable_language(C)
  add_executable(gen_constants gen/gen_constants.c)
  target_include_directories(gen_constants PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../..")
  message(STATUS "libastro: con440.c found; gen_constants enabled")
else()
  message(STATUS "libastro: con440.c not found; gen_constants disabled -> "
                 "constants test will skip")
endif()

# --- Tests -------------------------------------------------------------------
add_executable(test_ephemeris unit/test_ephemeris.cpp)
target_link_libraries(test_ephemeris PRIVATE astro::astro)
add_test(NAME ephemeris COMMAND test_ephemeris)

add_executable(test_replay unit/test_replay.cpp)
target_link_libraries(test_replay PRIVATE astro::astro)
add_test(NAME replay COMMAND test_replay "${_state_csv}")

add_executable(test_constants unit/test_constants.cpp)
target_link_libraries(test_constants PRIVATE astro::astro)
add_test(NAME constants COMMAND test_constants "${_const_csv}")

add_executable(test_place unit/test_place.cpp)
target_link_libraries(test_place PRIVATE astro::astro)
add_test(NAME place COMMAND test_place "${_place_csv}")

# Nutation series validation needs no ephemeris (pure functions of time).
add_executable(test_nutation unit/test_nutation.cpp)
target_link_libraries(test_nutation PRIVATE astro::astro)
add_test(NAME nutation COMMAND test_nutation "${_nut_csv}")

# equ2hor validation needs no ephemeris (Earth-orientation + geometry).
add_executable(test_hor unit/test_hor.cpp)
target_link_libraries(test_hor PRIVATE astro::astro)
add_test(NAME hor COMMAND test_hor "${_hor_csv}")

# Civil-time utility: self-checks always run; calendar vs NOVAS when available.
add_executable(test_time unit/test_time.cpp)
target_link_libraries(test_time PRIVATE astro::astro)
add_test(NAME time COMMAND test_time "${_time_csv}")

# Generation fixtures: produce the CSVs from the currently selected ephemeris
# before their consumers run. Wired only when the generator and a fetched
# ephemeris are both present; otherwise the consuming test finds no input file
# and skips, keeping CI green without the NOVAS build or the ephemeris download.
if(TARGET gen_vectors AND EXISTS "${_jpleph}")
  add_test(NAME gen_state COMMAND gen_vectors "${_jpleph}" "${_state_csv}")
  set_tests_properties(gen_state PROPERTIES FIXTURES_SETUP STATE_VECTORS)
  set_tests_properties(replay PROPERTIES FIXTURES_REQUIRED STATE_VECTORS)
endif()

if(TARGET gen_place AND EXISTS "${_jpleph}")
  add_test(NAME gen_place COMMAND gen_place "${_jpleph}" "${_place_csv}")
  set_tests_properties(gen_place PROPERTIES FIXTURES_SETUP PLACE_VECTORS)
  set_tests_properties(place PROPERTIES FIXTURES_REQUIRED PLACE_VECTORS)
endif()

# Nutation fixture needs only the generator (no ephemeris).
if(TARGET gen_nutation)
  add_test(NAME gen_nut COMMAND gen_nutation "${_nut_csv}")
  set_tests_properties(gen_nut PROPERTIES FIXTURES_SETUP NUTATION)
  set_tests_properties(nutation PROPERTIES FIXTURES_REQUIRED NUTATION)
endif()

if(TARGET gen_hor)
  add_test(NAME gen_hor COMMAND gen_hor "${_hor_csv}")
  set_tests_properties(gen_hor PROPERTIES FIXTURES_SETUP HORIZON)
  set_tests_properties(hor PROPERTIES FIXTURES_REQUIRED HORIZON)
endif()

if(TARGET gen_time)
  add_test(NAME gen_time COMMAND gen_time "${_time_csv}")
  set_tests_properties(gen_time PROPERTIES FIXTURES_SETUP TIMECAL)
  set_tests_properties(time PROPERTIES FIXTURES_REQUIRED TIMECAL)
endif()

if(TARGET gen_constants AND EXISTS "${_jpleph}")
  add_test(NAME gen_const COMMAND gen_constants "${_const_csv}")
  set_tests_properties(gen_const PROPERTIES FIXTURES_SETUP CONSTANTS_REF)
  set_tests_properties(constants PROPERTIES FIXTURES_REQUIRED CONSTANTS_REF)
endif()

if(EXISTS "${_jpleph}")
  set_tests_properties(ephemeris replay constants place PROPERTIES
    ENVIRONMENT "LIBASTRO_EPHEMERIS=${_jpleph}")
endif()
