// Validate astro::equ2hor against the NOVAS reference (gen_hor). Same algorithm
// => round-off agreement. Needs no ephemeris. Skips (exit 0) if the reference
// file is absent.
//
// Usage: test_hor <hor.csv>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "astro/reductions.hpp"

namespace {
constexpr double kTol = 1e-9;  // degrees / hours; same code path as the oracle

double circ_diff(double a, double b, double period) {
  double d = std::fabs(a - b);
  if (d > period / 2.0) d = period - d;
  return d;
}
}  // namespace

int main(int argc, char** argv) {
  const char* csv_path = (argc > 1) ? argv[1] : std::getenv("LIBASTRO_HOR");
  if (!csv_path) {
    std::fprintf(stderr, "SKIP hor: no reference file (arg or LIBASTRO_HOR)\n");
    return 0;
  }
  std::ifstream csv(csv_path);
  if (!csv) {
    std::fprintf(stderr, "SKIP hor: cannot open %s\n", csv_path);
    return 0;
  }

  std::string line;
  std::getline(csv, line);  // header

  long rows = 0, failures = 0;
  double max_zd = 0, max_az = 0, max_rar = 0, max_decr = 0;

  while (std::getline(csv, line)) {
    if (line.empty()) continue;
    int acc = 0, ref = 0;
    double jd = 0, dt = 0, lat = 0, lon = 0, h = 0, temp = 0, pres = 0;
    double xp = 0, yp = 0, ra = 0, dec = 0, zd = 0, az = 0, rar = 0, decr = 0;
    if (std::sscanf(line.c_str(),
                    "%d,%d,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,"
                    "%lf,%lf,%lf,%lf",
                    &acc, &ref, &jd, &dt, &lat, &lon, &h, &temp, &pres, &xp,
                    &yp, &ra, &dec, &zd, &az, &rar, &decr) != 17) {
      std::fprintf(stderr, "FAIL hor: malformed row: %s\n", line.c_str());
      ++failures;
      continue;
    }
    ++rows;

    const auto accuracy =
        acc == 0 ? astro::Accuracy::full : astro::Accuracy::reduced;
    const auto refraction = static_cast<astro::Refraction>(ref);
    auto got = astro::equ2hor(
        astro::Ut1Instant{astro::JulianDate{jd, 0.0}}, astro::DeltaT{dt},
        accuracy, astro::PolarMotion{xp, yp},
        astro::SurfaceObserver{lat, lon, h, temp, pres}, ra, dec, refraction);

    const double ezd = std::fabs(got.zenith_distance_deg - zd);
    const double eaz = circ_diff(got.azimuth_deg, az, 360.0);
    const double erar = circ_diff(got.ra_refracted_hours, rar, 24.0);
    const double edecr = std::fabs(got.dec_refracted_deg - decr);
    max_zd = std::fmax(max_zd, ezd);
    max_az = std::fmax(max_az, eaz);
    max_rar = std::fmax(max_rar, erar);
    max_decr = std::fmax(max_decr, edecr);

    if (ezd > kTol || eaz > kTol || erar > kTol || edecr > kTol) {
      std::fprintf(stderr,
                   "FAIL hor: acc=%d ref=%d jd=%.4f ra=%.1f dec=%.1f\n"
                   "  got zd=%.12f az=%.12f rar=%.12f decr=%.12f\n"
                   "  exp zd=%.12f az=%.12f rar=%.12f decr=%.12f\n",
                   acc, ref, jd, ra, dec, got.zenith_distance_deg,
                   got.azimuth_deg, got.ra_refracted_hours,
                   got.dec_refracted_deg, zd, az, rar, decr);
      ++failures;
    }
  }

  std::fprintf(stderr,
               "hor: %ld rows, %ld failures; max |dzd|=%.2e |daz|=%.2e "
               "|drar|=%.2e |ddecr|=%.2e\n",
               rows, failures, max_zd, max_az, max_rar, max_decr);
  return failures == 0 ? 0 : 1;
}
