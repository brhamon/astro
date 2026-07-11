# Verification has two tiers (see README "Verifying the results"):
#
#   * Oracle present (parent-repo NOVAS build): CTest fixtures regenerate
#     reference vectors from *your* NOVAS-C across the full ephemeris range and
#     the tests replay them -- an independent, full-range equivalence proof.
#   * Oracle absent: the numeric tests replay a small committed *golden* vector
#     set (a NOVAS-C slice, in vectors/golden_*.csv) instead -- a build-sanity
#     check that your toolchain computes the right numbers, no NOVAS needed.
#
# Either way the comparison is tolerance-based, so platform round-off is fine.

set(_jpleph "${CMAKE_CURRENT_SOURCE_DIR}/../data/JPLEPH")

# Regenerated (full-range) vectors -> build tree, never committed.
set(_state_csv "${CMAKE_CURRENT_BINARY_DIR}/state_vectors.csv")
set(_const_csv "${CMAKE_CURRENT_BINARY_DIR}/constants_ref.csv")
set(_place_csv "${CMAKE_CURRENT_BINARY_DIR}/place.csv")
set(_star_csv "${CMAKE_CURRENT_BINARY_DIR}/star.csv")
set(_nut_csv "${CMAKE_CURRENT_BINARY_DIR}/nutation.csv")
set(_hor_csv "${CMAKE_CURRENT_BINARY_DIR}/hor.csv")
set(_time_csv "${CMAKE_CURRENT_BINARY_DIR}/time.csv")

# Committed golden slices -> replayed when the oracle is unavailable.
set(_gold "${CMAKE_CURRENT_SOURCE_DIR}/vectors")

# --- Generators (oracles) ----------------------------------------------------
set(_novas_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../novasc3.1")
if(EXISTS "${_novas_dir}/novas.a")
  enable_language(C)
  foreach(g gen_vectors gen_place gen_star gen_nutation gen_hor gen_time)
    add_executable(${g} gen/${g}.c)
    target_include_directories(${g} PRIVATE "${_novas_dir}")
    target_link_libraries(${g} PRIVATE "${_novas_dir}/novas.a" m)
  endforeach()
  message(STATUS "libastro: NOVAS oracle found; full-range regeneration enabled")
else()
  message(STATUS "libastro: NOVAS not built; numeric tests replay committed "
                 "golden vectors (build-sanity mode)")
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
  message(STATUS "libastro: con440.c not found; constants test will skip")
endif()

# --- Test executables --------------------------------------------------------
foreach(t ephemeris replay constants place star nutation hor time)
  add_executable(test_${t} unit/test_${t}.cpp)
  target_link_libraries(test_${t} PRIVATE astro::astro)
endforeach()

add_test(NAME ephemeris COMMAND test_ephemeris)

# --- Oracle-backed numeric tests: regenerate if possible, else golden --------
# each: gen target, regenerated CSV, golden CSV, fixture name
macro(oracle_test name gentgt regen golden fixture)
  if(TARGET ${gentgt} AND EXISTS "${_jpleph}")
    add_test(NAME gen_${name} COMMAND ${gentgt} "${_jpleph}" "${regen}")
    set_tests_properties(gen_${name} PROPERTIES FIXTURES_SETUP ${fixture})
    add_test(NAME ${name} COMMAND test_${name} "${regen}")
    set_tests_properties(${name} PROPERTIES FIXTURES_REQUIRED ${fixture})
  else()
    add_test(NAME ${name} COMMAND test_${name} "${golden}")
  endif()
endmacro()

# same, for generators that take no ephemeris argument (nutation, hor)
macro(oracle_test_noeph name gentgt regen golden fixture)
  if(TARGET ${gentgt})
    add_test(NAME gen_${name} COMMAND ${gentgt} "${regen}")
    set_tests_properties(gen_${name} PROPERTIES FIXTURES_SETUP ${fixture})
    add_test(NAME ${name} COMMAND test_${name} "${regen}")
    set_tests_properties(${name} PROPERTIES FIXTURES_REQUIRED ${fixture})
  else()
    add_test(NAME ${name} COMMAND test_${name} "${golden}")
  endif()
endmacro()

oracle_test(replay gen_vectors "${_state_csv}" "${_gold}/golden_state_de440.csv" STATE_VECTORS)
oracle_test(place  gen_place   "${_place_csv}" "${_gold}/golden_place_de440.csv" PLACE_VECTORS)
oracle_test(star   gen_star    "${_star_csv}"  "${_gold}/golden_star_de440.csv"  STAR_VECTORS)
oracle_test_noeph(nutation gen_nutation "${_nut_csv}" "${_gold}/golden_nutation.csv" NUTATION)
oracle_test_noeph(hor      gen_hor      "${_hor_csv}" "${_gold}/golden_hor.csv"      HORIZON)

# constants validates against con440.c (committed), so it runs without NOVAS.
if(TARGET gen_constants AND EXISTS "${_jpleph}")
  add_test(NAME gen_const COMMAND gen_constants "${_const_csv}")
  set_tests_properties(gen_const PROPERTIES FIXTURES_SETUP CONSTANTS_REF)
  add_test(NAME constants COMMAND test_constants "${_const_csv}")
  set_tests_properties(constants PROPERTIES FIXTURES_REQUIRED CONSTANTS_REF)
else()
  add_test(NAME constants COMMAND test_constants "${_const_csv}")  # skips w/o ref
endif()

# time: NOVAS-free self-checks always run; calendar<->JD checked vs NOVAS when built.
if(TARGET gen_time)
  add_test(NAME gen_time COMMAND gen_time "${_time_csv}")
  set_tests_properties(gen_time PROPERTIES FIXTURES_SETUP TIMECAL)
  add_test(NAME time COMMAND test_time "${_time_csv}")
  set_tests_properties(time PROPERTIES FIXTURES_REQUIRED TIMECAL)
else()
  add_test(NAME time COMMAND test_time)  # self-checks only
endif()

# Tests that open the ephemeris need its path.
if(EXISTS "${_jpleph}")
  set_tests_properties(ephemeris replay constants place star PROPERTIES
    ENVIRONMENT "LIBASTRO_EPHEMERIS=${_jpleph}")
endif()
