// Replay the NOVAS place() vectors for stellar objects through astro::place()
// and require the sky position + radial velocity to match. Same algorithm +
// constants => round-off agreement. Skips (exit 0) if the ephemeris or vector
// file is absent.
//
// The star table is duplicated by index from gen_star.c; keep them in sync.
//
// Usage: test_star <star.csv>   (ephemeris via env LIBASTRO_EPHEMERIS)

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "astro/reductions.hpp"

namespace {

// Mirror of gen_star.c's table (ra_hours, dec_deg, pm_ra, pm_dec, parallax, rv).
constexpr std::array<astro::Star, 4> kStars = {{
    {2.5303010278, 89.2641094444, 3442.95, -11.8, 7.56, -17.4},
    {17.9633, 4.6933, -798.58, 10328.12, 546.98, -110.6},
    {12.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {6.7525, -16.7161, -546.0, -1223.0, 379.0, -5.5}}};

constexpr double kRaTol = 1e-9;    // hours
constexpr double kDecTol = 1e-8;   // degrees
constexpr double kRhatTol = 1e-11;
constexpr double kRvTol = 1e-9;    // km/s

}  // namespace

int main(int argc, char** argv) {
  const char* eph_path = std::getenv("LIBASTRO_EPHEMERIS");
  const char* csv_path = (argc > 1) ? argv[1] : std::getenv("LIBASTRO_STAR");
  if (!eph_path || !csv_path) {
    std::fprintf(stderr, "SKIP star: need LIBASTRO_EPHEMERIS and a vectors "
                         "file (arg or LIBASTRO_STAR)\n");
    return 0;
  }
  std::ifstream csv(csv_path);
  if (!csv) {
    std::fprintf(stderr, "SKIP star: cannot open %s\n", csv_path);
    return 0;
  }
  auto eph = astro::Ephemeris::open(eph_path);
  if (!eph) {
    std::fprintf(stderr, "FAIL star: open(%s): %s\n", eph_path,
                 std::string(astro::to_string(eph.error())).c_str());
    return 1;
  }

  std::string line;
  std::getline(csv, line);  // header

  long rows = 0, failures = 0;
  double max_ra = 0, max_dec = 0, max_rhat = 0, max_rv = 0;

  while (std::getline(csv, line)) {
    if (line.empty()) continue;
    int cs = 0, acc = 0, star = 0, where = 0;
    double lat = 0, lon = 0, height = 0, dt = 0;
    double jd = 0, ra = 0, dec = 0, dis = 0, rh[3] = {}, rv = 0;
    if (std::sscanf(line.c_str(),
                    "%d,%d,%d,%d,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf",
                    &cs, &acc, &star, &where, &lat, &lon, &height, &dt, &jd, &ra,
                    &dec, &dis, &rh[0], &rh[1], &rh[2], &rv) != 16) {
      std::fprintf(stderr, "FAIL star: malformed row: %s\n", line.c_str());
      ++failures;
      continue;
    }
    if (star < 0 || star >= static_cast<int>(kStars.size())) {
      std::fprintf(stderr, "FAIL star: bad star index %d\n", star);
      ++failures;
      continue;
    }
    ++rows;

    const auto sys = static_cast<astro::CoordSys>(cs);
    const auto accuracy =
        acc == 0 ? astro::Accuracy::full : astro::Accuracy::reduced;
    const astro::TtInstant tt{astro::JulianDate{jd, 0.0}};
    const astro::Star& s = kStars[static_cast<std::size_t>(star)];
    auto sp = where == 1
                  ? astro::place(*eph, s, tt, astro::DeltaT{dt},
                                 astro::SurfaceObserver{lat, lon, height, 10.0,
                                                        1010.0},
                                 sys, accuracy)
                  : astro::place(*eph, s, tt, astro::DeltaT{dt}, sys, accuracy);
    if (!sp) {
      std::fprintf(stderr, "FAIL star: place(cs=%d acc=%d star=%d): %s\n", cs,
                   acc, star, std::string(astro::to_string(sp.error())).c_str());
      ++failures;
      continue;
    }

    double dra = std::fabs(sp->ra_hours - ra);
    if (dra > 12.0) dra = 24.0 - dra;
    const double ddec = std::fabs(sp->dec_deg - dec);
    double drhat = 0.0;
    for (int i = 0; i < 3; ++i)
      drhat = std::fmax(drhat, std::fabs(sp->r_hat[i] - rh[i]));
    const double drv = std::fabs(sp->radial_velocity_km_s - rv);

    max_ra = std::fmax(max_ra, dra);
    max_dec = std::fmax(max_dec, ddec);
    max_rhat = std::fmax(max_rhat, drhat);
    max_rv = std::fmax(max_rv, drv);

    if (dra > kRaTol || ddec > kDecTol || drhat > kRhatTol || drv > kRvTol) {
      std::fprintf(stderr,
                   "FAIL star: mismatch cs=%d acc=%d star=%d where=%d jd=%.6f\n"
                   "  got ra=%.12f dec=%.12f rv=%.9f\n"
                   "  exp ra=%.12f dec=%.12f rv=%.9f\n",
                   cs, acc, star, where, jd, sp->ra_hours, sp->dec_deg,
                   sp->radial_velocity_km_s, ra, dec, rv);
      ++failures;
    }
  }

  std::fprintf(stderr,
               "star: %ld rows, %ld failures; max |dra|=%.2e h |ddec|=%.2e deg "
               "|drhat|=%.2e |drv|=%.2e km/s\n",
               rows, failures, max_ra, max_dec, max_rhat, max_rv);
  return failures == 0 ? 0 : 1;
}
