// Validate astro::equ_to_ecl_of_date against NOVAS equ2ecl (gen_ecl). Same
// obliquity + rotation => round-off agreement. No ephemeris. Skips (exit 0) if
// the reference file is absent.
//
// Usage: test_ecl <ecl.csv>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "astro/phenomena.hpp"

namespace {
constexpr double kTol = 1e-9;  // degrees
double circ_diff(double a, double b, double period) {
  double d = std::fabs(a - b);
  if (d > period / 2.0) d = period - d;
  return d;
}
}  // namespace

int main(int argc, char** argv) {
  const char* csv_path = (argc > 1) ? argv[1] : std::getenv("LIBASTRO_ECL");
  if (!csv_path) {
    std::fprintf(stderr, "SKIP ecl: no reference file (arg or LIBASTRO_ECL)\n");
    return 0;
  }
  std::ifstream csv(csv_path);
  if (!csv) {
    std::fprintf(stderr, "SKIP ecl: cannot open %s\n", csv_path);
    return 0;
  }

  std::string line;
  std::getline(csv, line);  // header
  long rows = 0, failures = 0;
  double max_lon = 0, max_lat = 0;
  while (std::getline(csv, line)) {
    if (line.empty()) continue;
    double jd = 0, ra = 0, dec = 0, elon = 0, elat = 0;
    if (std::sscanf(line.c_str(), "%lf,%lf,%lf,%lf,%lf", &jd, &ra, &dec, &elon,
                    &elat) != 5) {
      std::fprintf(stderr, "FAIL ecl: malformed row: %s\n", line.c_str());
      ++failures;
      continue;
    }
    ++rows;
    double my_lon, my_lat;
    astro::equ_to_ecl_of_date(jd, ra, dec, my_lon, my_lat);
    const double dlon = circ_diff(my_lon, elon, 360.0);
    const double dlat = std::fabs(my_lat - elat);
    max_lon = std::fmax(max_lon, dlon);
    max_lat = std::fmax(max_lat, dlat);
    if (dlon > kTol || dlat > kTol) {
      std::fprintf(stderr,
                   "FAIL ecl: jd=%.4f ra=%.2f dec=%.2f  got (%.10f,%.10f) "
                   "exp (%.10f,%.10f)\n",
                   jd, ra, dec, my_lon, my_lat, elon, elat);
      ++failures;
    }
  }
  std::fprintf(stderr, "ecl: %ld rows, %ld failures; max |dlon|=%.2e |dlat|=%.2e\n",
               rows, failures, max_lon, max_lat);
  return failures == 0 ? 0 : 1;
}
