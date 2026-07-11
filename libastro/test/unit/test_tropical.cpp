// Validate the tropical_moments generator by invariants + self-consistency
// (no NOVAS oracle -- equinoxes/solstices aren't NOVAS functions). Needs the
// ephemeris (place); skips (exit 0) if absent.
//
// Checks: seasons cycle March->June->Sept->Dec; times strictly increasing and
// ~one quarter-year apart; the Sun's apparent longitude at each moment equals
// the season's target (0/90/180/270); backward reproduces the forward instants.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ranges>
#include <string>
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

double target_deg(astro::Season s) { return 90.0 * static_cast<int>(s); }
}  // namespace

int main() {
  const char* eph_path = std::getenv("LIBASTRO_EPHEMERIS");
  if (!eph_path) {
    std::fprintf(stderr, "SKIP tropical: set LIBASTRO_EPHEMERIS\n");
    return 0;
  }
  auto eph = astro::Ephemeris::open(eph_path);
  if (!eph) { std::fprintf(stderr, "FAIL tropical: open\n"); return 1; }

  const auto start = astro::utc_time_scales(2000, 3, 1, 0.0).tt;

  std::vector<astro::SeasonalMoment> fwd;
  for (auto m : astro::tropical_moments(*eph, start) | std::views::take(12))
    fwd.push_back(m);
  CHECK(fwd.size() == 12);

  double max_lon_err = 0.0;
  for (std::size_t i = 0; i < fwd.size(); ++i) {
    // self-consistency: Sun's apparent longitude == the season target
    double lam = astro::sun_apparent_longitude(*eph, fwd[i].time);
    double d = std::fabs(lam - target_deg(fwd[i].season));
    if (d > 180.0) d = 360.0 - d;
    max_lon_err = std::fmax(max_lon_err, d);
    CHECK(d < 1.0e-4);
    if (i > 0) {
      // seasons cycle by +1 (mod 4); spacing ~ quarter year
      CHECK((static_cast<int>(fwd[i].season) -
             static_cast<int>(fwd[i - 1].season) + 4) % 4 == 1);
      double dt = fwd[i].time.jd.value() - fwd[i - 1].time.jd.value();
      CHECK(dt > 85.0 && dt < 95.0);
    }
  }

  // backward from just past the 6th moment should reproduce moments 6..1.
  const astro::TtInstant later{astro::JulianDate{fwd[5].time.jd.value() + 5.0}};
  std::vector<astro::SeasonalMoment> bwd;
  for (auto m : astro::tropical_moments(*eph, later, astro::Direction::backward) |
                    std::views::take(6))
    bwd.push_back(m);
  CHECK(bwd.size() == 6);
  double max_bwd_err = 0.0;
  for (std::size_t i = 0; i < bwd.size(); ++i) {
    CHECK(bwd[i].season == fwd[5 - i].season);
    double e = std::fabs(bwd[i].time.jd.value() - fwd[5 - i].time.jd.value());
    max_bwd_err = std::fmax(max_bwd_err, e);
    CHECK(e < 1.0e-6);
  }

  std::fprintf(stderr,
               "tropical: %zu forward, %zu backward; max |dlon|=%.2e deg, "
               "max backward mismatch=%.2e d; %d failures\n",
               fwd.size(), bwd.size(), max_lon_err, max_bwd_err, g_fail);
  return g_fail == 0 ? 0 : 1;
}
