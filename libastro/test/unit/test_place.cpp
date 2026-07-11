// Replay the NOVAS place() vectors through astro::place() and require the sky
// position to match. Same algorithm + constants => round-off agreement
// (research doc 3.5). Skips (exit 0) if the ephemeris or vector file is absent.
//
// Usage: test_place <place.csv>   (ephemeris via env LIBASTRO_EPHEMERIS)

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "astro/reductions.hpp"

namespace {

// ra in hours, dec in degrees. Same code path as the oracle -> tiny residual;
// tolerances catch real regressions while allowing FP reassociation.
constexpr double kRaTol = 1e-9;    // hours (~5e-5 arcsec)
constexpr double kDecTol = 1e-8;   // degrees (~3.6e-5 arcsec)
constexpr double kDisRelTol = 1e-11;
constexpr double kRhatTol = 1e-11;

// NOVAS object number -> astro::Point.
astro::Point to_point(int novas_number) {
  switch (novas_number) {
    case 10: return astro::Point::sun;
    case 11: return astro::Point::moon;
    default: return static_cast<astro::Point>(novas_number - 1);  // 1..9 -> 0..8
  }
}

}  // namespace

int main(int argc, char** argv) {
  const char* eph_path = std::getenv("LIBASTRO_EPHEMERIS");
  const char* csv_path = (argc > 1) ? argv[1] : std::getenv("LIBASTRO_PLACE");

  if (!eph_path || !csv_path) {
    std::fprintf(stderr, "SKIP place: need LIBASTRO_EPHEMERIS and a vectors "
                         "file (arg or LIBASTRO_PLACE)\n");
    return 0;
  }

  std::ifstream csv(csv_path);
  if (!csv) {
    std::fprintf(stderr, "SKIP place: cannot open %s\n", csv_path);
    return 0;
  }

  auto eph = astro::Ephemeris::open(eph_path);
  if (!eph) {
    std::fprintf(stderr, "FAIL place: open(%s): %s\n", eph_path,
                 std::string(astro::to_string(eph.error())).c_str());
    return 1;
  }

  std::string line;
  std::getline(csv, line);  // header

  long rows = 0, failures = 0;
  double max_ra = 0, max_dec = 0, max_dis_rel = 0, max_rhat = 0;

  while (std::getline(csv, line)) {
    if (line.empty()) continue;
    int cs = 0, acc = 0, body = 0, where = 0;
    double lat = 0, lon = 0, height = 0, dt = 0;
    double jd = 0, ra = 0, dec = 0, dis = 0, rh[3] = {};
    if (std::sscanf(line.c_str(),
                    "%d,%d,%d,%d,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf",
                    &cs, &acc, &body, &where, &lat, &lon, &height, &dt, &jd, &ra,
                    &dec, &dis, &rh[0], &rh[1], &rh[2]) != 15) {
      std::fprintf(stderr, "FAIL place: malformed row: %s\n", line.c_str());
      ++failures;
      continue;
    }
    ++rows;

    const auto sys = static_cast<astro::CoordSys>(cs);
    const auto accuracy =
        acc == 0 ? astro::Accuracy::full : astro::Accuracy::reduced;
    const astro::TtInstant tt{astro::JulianDate{jd, 0.0}};
    auto sp = where == 1
                  ? astro::place(*eph, to_point(body), tt, astro::DeltaT{dt},
                                 astro::SurfaceObserver{lat, lon, height, 10.0,
                                                        1010.0},
                                 sys, accuracy)
                  : astro::place(*eph, to_point(body), tt, astro::DeltaT{dt},
                                 sys, accuracy);
    if (!sp) {
      std::fprintf(stderr,
                   "FAIL place: place(cs=%d acc=%d body=%d where=%d jd=%.6f): %s\n",
                   cs, acc, body, where, jd,
                   std::string(astro::to_string(sp.error())).c_str());
      ++failures;
      continue;
    }

    // RA wraps at 24h; compare on the circle.
    double dra = std::fabs(sp->ra_hours - ra);
    if (dra > 12.0) dra = 24.0 - dra;
    const double ddec = std::fabs(sp->dec_deg - dec);
    const double ddis = dis != 0.0 ? std::fabs(sp->distance_au - dis) / dis : 0.0;
    double drhat = 0.0;
    for (int i = 0; i < 3; ++i)
      drhat = std::fmax(drhat, std::fabs(sp->r_hat[i] - rh[i]));

    max_ra = std::fmax(max_ra, dra);
    max_dec = std::fmax(max_dec, ddec);
    max_dis_rel = std::fmax(max_dis_rel, ddis);
    max_rhat = std::fmax(max_rhat, drhat);

    if (dra > kRaTol || ddec > kDecTol || ddis > kDisRelTol ||
        drhat > kRhatTol) {
      std::fprintf(stderr,
                   "FAIL place: mismatch cs=%d acc=%d body=%d jd=%.6f\n"
                   "  got ra=%.12f dec=%.12f dis=%.12f\n"
                   "  exp ra=%.12f dec=%.12f dis=%.12f\n",
                   cs, acc, body, jd, sp->ra_hours, sp->dec_deg,
                   sp->distance_au, ra, dec, dis);
      ++failures;
    }
  }

  std::fprintf(stderr,
               "place: %ld rows, %ld failures; max |dra|=%.2e h |ddec|=%.2e deg "
               "|ddis|/dis=%.2e |drhat|=%.2e\n",
               rows, failures, max_ra, max_dec, max_dis_rel, max_rhat);
  return failures == 0 ? 0 : 1;
}
