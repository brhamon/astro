// Validate the apsides generator by self-consistency + known Earth values
// (apsides aren't NOVAS functions, so there's no bit-for-bit oracle): at each
// event the radial velocity r.v = 0 and the distance is a local extremum;
// Earth's perihelion/aphelion distances and the peri/apo alternation and
// spacing are as expected; backward reproduces forward. Needs the ephemeris.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ranges>
#include <vector>

#include "astro/ephemeris.hpp"
#include "astro/phenomena.hpp"
#include "astro/time.hpp"

namespace {
int g_fail = 0;
#define CHECK(c)                                                          \
  do {                                                                    \
    if (!(c)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++g_fail; } \
  } while (0)

// Heliocentric distance (AU) and radial velocity r.v (AU^2/day) at TT jd.
void dist_rv(const astro::Ephemeris& e, astro::Point b, astro::Point c,
             double tt_jd, double& dist, double& rv) {
  const auto tdb = astro::tdb_from_tt(astro::TtInstant{astro::JulianDate{tt_jd}});
  auto st = e.state(b, c, tdb, astro::Units::au);
  const auto& p = st->position;
  const auto& v = st->velocity;
  dist = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
  rv = p[0] * v[0] + p[1] * v[1] + p[2] * v[2];
}
}  // namespace

int main() {
  const char* eph_path = std::getenv("LIBASTRO_EPHEMERIS");
  if (!eph_path) { std::fprintf(stderr, "SKIP apsides: set LIBASTRO_EPHEMERIS\n"); return 0; }
  auto eph = astro::Ephemeris::open(eph_path);
  if (!eph) { std::fprintf(stderr, "FAIL apsides: open\n"); return 1; }

  const auto B = astro::Point::earth, C = astro::Point::sun;
  const auto start = astro::utc_time_scales(2026, 1, 1, 0.0).tt;

  std::vector<astro::ApsisEvent> fwd;
  for (auto a : astro::apsides(*eph, B, C, start) | std::views::take(6))
    fwd.push_back(a);
  CHECK(fwd.size() == 6);

  double max_rv = 0.0, prev = -1e18;
  for (std::size_t i = 0; i < fwd.size(); ++i) {
    const double t = fwd[i].time.jd.value();
    CHECK(t > prev); prev = t;
    double dist, rv, dm, dp, junk;
    dist_rv(*eph, B, C, t, dist, rv);
    dist_rv(*eph, B, C, t - 0.5, dm, junk);
    dist_rv(*eph, B, C, t + 0.5, dp, junk);
    max_rv = std::fmax(max_rv, std::fabs(rv));

    CHECK(std::fabs(rv) < 1.0e-5);                       // r.v == 0 at apsis
    CHECK(std::fabs(dist - fwd[i].distance_au) < 1e-9);  // reported distance
    if (fwd[i].kind == astro::Apsis::periapsis) {
      CHECK(dist > 0.982 && dist < 0.985);               // Earth perihelion
      CHECK(dist < dm && dist < dp);                     // distance minimum
    } else {
      CHECK(dist > 1.015 && dist < 1.018);               // Earth aphelion
      CHECK(dist > dm && dist > dp);                     // distance maximum
    }
    if (i > 0) {
      CHECK(fwd[i].kind != fwd[i - 1].kind);             // peri/apo alternate
      const double gap = fwd[i].time.jd.value() - fwd[i - 1].time.jd.value();
      CHECK(gap > 150.0 && gap < 210.0);                 // ~half year
    }
  }

  // Backward from just past the 4th apsis reproduces apsides 4..1.
  const astro::TtInstant later{astro::JulianDate{fwd[3].time.jd.value() + 1.0}};
  std::vector<astro::ApsisEvent> bwd;
  for (auto a : astro::apsides(*eph, B, C, later, astro::Direction::backward) |
                    std::views::take(4))
    bwd.push_back(a);
  CHECK(bwd.size() == 4);
  double max_bwd = 0.0;
  for (std::size_t i = 0; i < bwd.size(); ++i) {
    CHECK(bwd[i].kind == fwd[3 - i].kind);
    max_bwd = std::fmax(max_bwd, std::fabs(bwd[i].time.jd.value() -
                                           fwd[3 - i].time.jd.value()));
  }
  CHECK(max_bwd < 1e-4);

  std::fprintf(stderr,
               "apsides: %zu fwd, %zu bwd; max |r.v|=%.2e AU^2/day, "
               "max bwd mismatch=%.2e d; %d failures\n",
               fwd.size(), bwd.size(), max_rv, max_bwd, g_fail);
  return g_fail == 0 ? 0 : 1;
}
