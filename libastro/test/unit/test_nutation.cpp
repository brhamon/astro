// Validate astro::nutation_angles / mean_obliquity against the NOVAS reference
// series (gen_nutation). Same tables + arithmetic => round-off agreement. Needs
// no ephemeris. Skips (exit 0) if the reference file is absent.
//
// Usage: test_nutation <nutation.csv>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "astro/frames.hpp"

namespace {
constexpr double kT0 = 2451545.0;
// Same tables/arithmetic as the oracle; residual is round-off. Angles are in
// arcseconds; a genuine table/argument error deviates by >> 1e-4.
constexpr double kTol = 1e-7;
}  // namespace

int main(int argc, char** argv) {
  const char* csv_path = (argc > 1) ? argv[1] : std::getenv("LIBASTRO_NUTATION");
  if (!csv_path) {
    std::fprintf(stderr, "SKIP nutation: no reference file (arg or "
                         "LIBASTRO_NUTATION)\n");
    return 0;
  }
  std::ifstream csv(csv_path);
  if (!csv) {
    std::fprintf(stderr, "SKIP nutation: cannot open %s\n", csv_path);
    return 0;
  }

  std::string line;
  std::getline(csv, line);  // header

  long rows = 0, failures = 0;
  double max_dpsi = 0, max_deps = 0, max_mobl = 0;

  while (std::getline(csv, line)) {
    if (line.empty()) continue;
    int acc = 0;
    double jd = 0, dpsi = 0, deps = 0, mobl = 0;
    if (std::sscanf(line.c_str(), "%d,%lf,%lf,%lf,%lf", &acc, &jd, &dpsi, &deps,
                    &mobl) != 5) {
      std::fprintf(stderr, "FAIL nutation: malformed row: %s\n", line.c_str());
      ++failures;
      continue;
    }
    ++rows;

    const double t = (jd - kT0) / 36525.0;
    const auto accuracy =
        acc == 0 ? astro::Accuracy::full : astro::Accuracy::reduced;
    double gp, ge;
    astro::nutation_angles(t, accuracy, gp, ge);
    const double gm = astro::mean_obliquity(jd);

    const double edp = std::fabs(gp - dpsi);
    const double ede = std::fabs(ge - deps);
    const double emo = std::fabs(gm - mobl);
    max_dpsi = std::fmax(max_dpsi, edp);
    max_deps = std::fmax(max_deps, ede);
    max_mobl = std::fmax(max_mobl, emo);

    if (edp > kTol || ede > kTol || emo > kTol) {
      std::fprintf(stderr,
                   "FAIL nutation: acc=%d jd=%.6f\n"
                   "  got dpsi=%.12f deps=%.12f mobl=%.9f\n"
                   "  exp dpsi=%.12f deps=%.12f mobl=%.9f\n",
                   acc, jd, gp, ge, gm, dpsi, deps, mobl);
      ++failures;
    }
  }

  std::fprintf(stderr,
               "nutation: %ld rows, %ld failures; max |ddpsi|=%.2e "
               "|ddeps|=%.2e |dmobl|=%.2e (arcsec)\n",
               rows, failures, max_dpsi, max_deps, max_mobl);
  return failures == 0 ? 0 : 1;
}
