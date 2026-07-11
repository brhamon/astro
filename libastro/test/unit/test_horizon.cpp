// Validate the horizon_events generator by self-consistency (rise/set has no
// NOVAS oracle -- it's an almanac event, not a NOVAS function):
//   * events are strictly ordered in time;
//   * rise/set land on the chosen standard altitude h0;
//   * culminations sit on the meridian (azimuth ~ 0 or 180 deg);
//   * a backward stream reproduces the forward instants.
// Needs the ephemeris (place); skips (exit 0) if absent.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
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

double circ_dist(double a, double b, double period) {
  double d = std::fabs(a - b);
  return std::fmin(d, period - d);
}
}  // namespace

int main() {
  const char* eph_path = std::getenv("LIBASTRO_EPHEMERIS");
  if (!eph_path) {
    std::fprintf(stderr, "SKIP horizon: set LIBASTRO_EPHEMERIS\n");
    return 0;
  }
  auto eph = astro::Ephemeris::open(eph_path);
  if (!eph) { std::fprintf(stderr, "FAIL horizon: open\n"); return 1; }

  const astro::SurfaceObserver obs{47.6096694, -122.340412, 10.0, 15.0, 1026.4};
  const auto start = astro::utc_time_scales(2026, 1, 1, 0.0).tt;
  const auto horizon = astro::Horizon::sun_upper_limb;
  const double h0 = -0.8333;  // must match Horizon::sun_upper_limb

  std::vector<astro::SkyEvent> fwd;
  for (auto e : astro::horizon_events(*eph, astro::Point::sun, obs, start, horizon) |
                    std::views::take(12))
    fwd.push_back(e);
  CHECK(fwd.size() == 12);

  double prev = -std::numeric_limits<double>::infinity();
  double max_h0_err = 0.0, max_merid_err = 0.0;
  for (const auto& e : fwd) {
    CHECK(e.time.jd.value() > prev);
    prev = e.time.jd.value();
    if (e.kind == astro::EventKind::rise || e.kind == astro::EventKind::set) {
      max_h0_err = std::fmax(max_h0_err, std::fabs(e.altitude_deg - h0));
      CHECK(std::fabs(e.altitude_deg - h0) < 1.0e-3);
    } else {  // upper/lower transit: on the meridian (due north or due south)
      const double m = std::fmin(circ_dist(e.azimuth_deg, 0.0, 360.0),
                                 circ_dist(e.azimuth_deg, 180.0, 360.0));
      max_merid_err = std::fmax(max_merid_err, m);
      CHECK(m < 0.5);
    }
  }

  // Backward from just past the 6th event reproduces events 6..1, reversed.
  const astro::TtInstant later{astro::JulianDate{fwd[5].time.jd.value() + 0.01}};
  std::vector<astro::SkyEvent> bwd;
  for (auto e : astro::horizon_events(*eph, astro::Point::sun, obs, later, horizon,
                                      astro::Direction::backward) |
                    std::views::take(6))
    bwd.push_back(e);
  CHECK(bwd.size() == 6);
  double max_bwd = 0.0;
  for (std::size_t i = 0; i < bwd.size(); ++i) {
    CHECK(bwd[i].kind == fwd[5 - i].kind);
    const double d = std::fabs(bwd[i].time.jd.value() - fwd[5 - i].time.jd.value());
    max_bwd = std::fmax(max_bwd, d);
    CHECK(d < 1.0e-5);
  }

  std::fprintf(stderr,
               "horizon: %zu fwd, %zu bwd; max |alt-h0|=%.2e deg, "
               "max meridian dev=%.2e deg, max bwd mismatch=%.2e d; %d failures\n",
               fwd.size(), bwd.size(), max_h0_err, max_merid_err, max_bwd, g_fail);
  return g_fail == 0 ? 0 : 1;
}
