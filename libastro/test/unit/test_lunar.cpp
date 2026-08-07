// Validate the lunar_phases generator by invariants + self-consistency (no
// NOVAS oracle -- lunar phases aren't NOVAS functions). Needs the ephemeris
// (place); skips (exit 0) if absent.
//
// Checks: phases cycle new->first->full->last; times strictly increasing and
// ~a quarter-lunation apart; the Moon-Sun elongation at each moment equals the
// phase target (0/90/180/270); backward reproduces the forward instants.

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

double target_deg(astro::LunarPhase p) { return 90.0 * static_cast<int>(p); }

// Moon-Sun elongation in ecliptic longitude at a moment, [0,360).
double elongation(const astro::Ephemeris& e, astro::TtInstant t) {
  double d = astro::moon_apparent_longitude(e, t) - astro::sun_apparent_longitude(e, t);
  d = std::fmod(d, 360.0);
  if (d < 0.0) d += 360.0;
  return d;
}
}  // namespace

int main() {
  const char* eph_path = std::getenv("LIBASTRO_EPHEMERIS");
  if (!eph_path) { std::fprintf(stderr, "SKIP lunar: set LIBASTRO_EPHEMERIS\n"); return 0; }
  auto eph = astro::Ephemeris::open(eph_path);
  if (!eph) { std::fprintf(stderr, "FAIL lunar: open\n"); return 1; }

  const auto start = astro::utc_time_scales(2026, 1, 1, 0.0).tt;

  std::vector<astro::LunarPhaseMoment> fwd;
  for (auto m : astro::lunar_phases(*eph, start) | std::views::take(12))
    fwd.push_back(m);
  CHECK(fwd.size() == 12);

  double max_elong_err = 0.0, prev = -1e18;
  for (std::size_t i = 0; i < fwd.size(); ++i) {
    // self-consistency: elongation at the moment == the phase target
    double e = elongation(*eph, fwd[i].time);
    double d = std::fabs(e - target_deg(fwd[i].phase));
    if (d > 180.0) d = 360.0 - d;
    max_elong_err = std::fmax(max_elong_err, d);
    CHECK(d < 1.0e-3);

    CHECK(fwd[i].time.jd.value() > prev);  // strictly increasing
    prev = fwd[i].time.jd.value();

    if (i > 0) {
      // phases cycle new->first->full->last (+1 mod 4)
      CHECK((static_cast<int>(fwd[i].phase) -
             static_cast<int>(fwd[i - 1].phase) + 4) % 4 == 1);
      // ~quarter of a synodic month (varies with anomaly/evection)
      double dt = fwd[i].time.jd.value() - fwd[i - 1].time.jd.value();
      CHECK(dt > 5.5 && dt < 9.5);
    }
  }

  // Backward from just past the 5th moment reproduces moments 5..1.
  const astro::TtInstant later{astro::JulianDate{fwd[4].time.jd.value() + 1.0}};
  std::vector<astro::LunarPhaseMoment> bwd;
  for (auto m : astro::lunar_phases(*eph, later, astro::Direction::backward) |
                    std::views::take(5))
    bwd.push_back(m);
  CHECK(bwd.size() == 5);
  double max_bwd = 0.0;
  for (std::size_t i = 0; i < bwd.size(); ++i) {
    CHECK(bwd[i].phase == fwd[4 - i].phase);
    max_bwd = std::fmax(max_bwd, std::fabs(bwd[i].time.jd.value() -
                                           fwd[4 - i].time.jd.value()));
  }
  CHECK(max_bwd < 1e-4);

  std::fprintf(stderr,
               "lunar: %zu fwd, %zu bwd; max |elong-target|=%.2e deg, "
               "max bwd mismatch=%.2e d; %d failures\n",
               fwd.size(), bwd.size(), max_elong_err, max_bwd, g_fail);
  return g_fail == 0 ? 0 : 1;
}
