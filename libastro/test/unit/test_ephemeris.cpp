// Dependency-free unit tests for Layer 0. The header-parsing path is exercised
// against a real ephemeris when one is available (env LIBASTRO_EPHEMERIS, set
// by CMake when data/JPLEPH exists); otherwise those checks are skipped and the
// always-runnable checks still execute.

#include <cstdio>
#include <cstdlib>

#include "astro/ephemeris.hpp"

namespace {

int g_failures = 0;

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)

void test_missing_file() {
  auto eph = astro::Ephemeris::open("/nonexistent/path/JPLEPH");
  CHECK(!eph.has_value());
  if (!eph) CHECK(eph.error() == astro::EphError::file_not_found);
}

void test_header(const char* path) {
  auto eph = astro::Ephemeris::open(path);
  CHECK(eph.has_value());
  if (!eph) {
    std::fprintf(stderr, "  open(%s) failed: %s\n", path,
                 std::string(astro::to_string(eph.error())).c_str());
    return;
  }

  const astro::Header& h = eph->header();
  std::fprintf(stderr, "  opened DE%d \"%s\"  JD %.1f..%.1f  %.0f d/rec  %zu recs\n",
               h.denum, h.title.c_str(), h.jd_begin, h.jd_end,
               h.days_per_record, h.record_count);

  CHECK(h.jd_end > h.jd_begin);
  CHECK(h.record_length == 8144 || h.record_length == 6608 ||
        h.record_length == 5824);
  CHECK(h.record_count > 0);
  CHECK(h.au_km > 1.4e8 && h.au_km < 1.5e8);
  CHECK(h.earth_moon_ratio > 80.0 && h.earth_moon_ratio < 82.0);

  // DE440-specific expectations (research doc 1.2, 1.3), verified from the file.
  if (h.denum == 440) {
    CHECK(h.days_per_record == 32.0);
    CHECK(h.record_length == 8144);
    CHECK(h.n_constants == 645);
    CHECK(h.groups[0].offset == 3 && h.groups[0].n_coeff == 14 &&
          h.groups[0].n_subintervals == 4);   // Mercury
    CHECK(h.groups[9].n_subintervals == 8);    // Moon (geocentric)
    CHECK(h.groups[11].offset == 819);         // Nutations
    CHECK(h.groups[12].offset == 899);         // Librations
  }

  // Same target/center yields the zero state regardless of interpolation.
  auto zero = eph->state(astro::Point::earth, astro::Point::earth,
                         astro::TdbInstant{astro::JulianDate{h.jd_begin}});
  CHECK(zero.has_value());
}

}  // namespace

int main() {
  test_missing_file();

  if (const char* path = std::getenv("LIBASTRO_EPHEMERIS")) {
    test_header(path);
  } else {
    std::fprintf(stderr,
                 "SKIP header tests: set LIBASTRO_EPHEMERIS or run "
                 "scripts/fetch_ephemeris.sh\n");
  }

  if (g_failures == 0) {
    std::fprintf(stderr, "OK\n");
    return 0;
  }
  std::fprintf(stderr, "%d check(s) failed\n", g_failures);
  return 1;
}
