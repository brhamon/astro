// Validate the Constants parsed from the ephemeris's record 2 against the
// reference transcription in con440.c (via constants_de440.csv). Same source
// data -> exact agreement expected (research doc 1.6). Skips (exit 0) if the
// ephemeris or reference file is absent.
//
// Usage: test_constants <constants.csv>   (ephemeris via env LIBASTRO_EPHEMERIS)

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "astro/ephemeris.hpp"

int main(int argc, char** argv) {
  const char* eph_path = std::getenv("LIBASTRO_EPHEMERIS");
  const char* csv_path = (argc > 1) ? argv[1] : std::getenv("LIBASTRO_CONSTANTS");

  if (!eph_path || !csv_path) {
    std::fprintf(stderr, "SKIP constants: need LIBASTRO_EPHEMERIS and a "
                         "reference file (arg or LIBASTRO_CONSTANTS)\n");
    return 0;
  }

  std::ifstream csv(csv_path);
  if (!csv) {
    std::fprintf(stderr, "SKIP constants: cannot open %s\n", csv_path);
    return 0;
  }

  auto eph = astro::Ephemeris::open(eph_path);
  if (!eph) {
    std::fprintf(stderr, "FAIL constants: open(%s): %s\n", eph_path,
                 std::string(astro::to_string(eph.error())).c_str());
    return 1;
  }
  // con440.c is a DE440-specific transcription; only meaningful against DE440.
  if (eph->header().denum != 440) {
    std::fprintf(stderr,
                 "SKIP constants: reference is DE440 but ephemeris is DE%d\n",
                 eph->header().denum);
    return 0;
  }
  const astro::Constants& c = eph->constants();

  long rows = 0, failures = 0;
  double max_rel = 0.0;

  std::string line;
  std::getline(csv, line);  // header
  while (std::getline(csv, line)) {
    if (line.empty()) continue;
    const auto comma = line.find(',');
    if (comma == std::string::npos) continue;
    const std::string name = line.substr(0, comma);
    const double expected = std::strtod(line.c_str() + comma + 1, nullptr);
    ++rows;

    const auto got = c.get(name);
    if (!got) {
      std::fprintf(stderr, "FAIL constants: %s missing from ephemeris\n",
                   name.c_str());
      ++failures;
      continue;
    }

    const double diff = std::fabs(*got - expected);
    const double rel = expected != 0.0 ? diff / std::fabs(expected) : diff;
    max_rel = std::fmax(max_rel, rel);
    // Exact match expected (same doubles); allow a hair for any transcription
    // rounding in con440.c's decimal literals.
    if (!(diff == 0.0 || rel <= 1e-13)) {
      std::fprintf(stderr, "FAIL constants: %s got %.17g expected %.17g\n",
                   name.c_str(), *got, expected);
      ++failures;
    }
  }

  // Structural cross-checks against the parsed header.
  const astro::Header& h = eph->header();
  if (static_cast<long>(c.size()) != h.n_constants) {
    std::fprintf(stderr, "FAIL constants: parsed %zu but header NCON=%d\n",
                 c.size(), h.n_constants);
    ++failures;
  }
  if (static_cast<long>(c.size()) != rows) {
    std::fprintf(stderr, "FAIL constants: parsed %zu but reference has %ld\n",
                 c.size(), rows);
    ++failures;
  }
  if (auto au = c.au_km(); !au || std::fabs(*au - h.au_km) > 0.0) {
    std::fprintf(stderr, "FAIL constants: AU constant != header AU\n");
    ++failures;
  }
  if (auto emr = c.earth_moon_ratio();
      !emr || std::fabs(*emr - h.earth_moon_ratio) > 0.0) {
    std::fprintf(stderr, "FAIL constants: EMRAT constant != header EMRAT\n");
    ++failures;
  }

  std::fprintf(stderr,
               "constants: %ld reference rows, %zu parsed, %ld failures; "
               "max relative error %.3e\n",
               rows, c.size(), failures, max_rel);
  return failures == 0 ? 0 : 1;
}
