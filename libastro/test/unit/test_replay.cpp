// Replay test: feed every (units, target, center, jd_hi, jd_lo) row from a
// NOVAS-generated vector file through astro::Ephemeris::state() and require the
// position and velocity to match the oracle. Same algorithm + same ephemeris
// => agreement to round-off (research doc 3.5). Skips (exit 0) if the ephemeris
// or vector file is absent, so CI without the downloads still passes.
//
// Usage: test_replay <vectors.csv>   (ephemeris via env LIBASTRO_EPHEMERIS)

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "astro/ephemeris.hpp"

namespace {

// The C++ port reproduces NOVAS-C bit-for-bit on position and to ~1 ULP on
// velocity. A combined absolute/relative bound stays meaningful across both
// unit systems (AU values ~1e1, km values ~1e9) while sitting far below any
// physical significance; a real regression deviates by many orders more.
//   ok  <=>  |got - exp| <= atol + rtol * |exp|
constexpr double kAtol = 1e-9;
constexpr double kRtol = 1e-12;

bool close_enough(double got, double exp, double& rel_out) {
  const double diff = std::fabs(got - exp);
  const double scale = std::fabs(exp);
  rel_out = scale > 0.0 ? diff / scale : diff;
  return diff <= kAtol + kRtol * scale;
}

}  // namespace

int main(int argc, char** argv) {
  const char* eph_path = std::getenv("LIBASTRO_EPHEMERIS");
  const char* csv_path = (argc > 1) ? argv[1] : std::getenv("LIBASTRO_VECTORS");

  if (!eph_path || !csv_path) {
    std::fprintf(stderr, "SKIP replay: need LIBASTRO_EPHEMERIS and a vectors "
                         "file (arg or LIBASTRO_VECTORS)\n");
    return 0;
  }

  std::ifstream csv(csv_path);
  if (!csv) {
    std::fprintf(stderr, "SKIP replay: cannot open vectors file %s\n", csv_path);
    return 0;
  }

  auto eph = astro::Ephemeris::open(eph_path);
  if (!eph) {
    std::fprintf(stderr, "FAIL replay: open(%s): %s\n", eph_path,
                 std::string(astro::to_string(eph.error())).c_str());
    return 1;
  }

  std::string line;
  std::getline(csv, line);  // header row

  long rows = 0, failures = 0;
  double max_rel = 0.0;

  while (std::getline(csv, line)) {
    if (line.empty()) continue;
    int units = 0, tgt = 0, ctr = 0;
    double hi = 0, lo = 0, p[3] = {}, v[3] = {};
    if (std::sscanf(line.c_str(),
                    "%d,%d,%d,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf",
                    &units, &tgt, &ctr, &hi, &lo, &p[0], &p[1], &p[2], &v[0],
                    &v[1], &v[2]) != 11) {
      std::fprintf(stderr, "FAIL replay: malformed row: %s\n", line.c_str());
      ++failures;
      continue;
    }
    ++rows;

    const auto u = units == 1 ? astro::Units::km : astro::Units::au;
    auto st = eph->state(static_cast<astro::Point>(tgt),
                         static_cast<astro::Point>(ctr),
                         astro::TdbInstant{astro::JulianDate{hi, lo}}, u);
    if (!st) {
      std::fprintf(stderr, "FAIL replay: state(u=%d,%d,%d,%.9f): %s\n", units,
                   tgt, ctr, hi + lo,
                   std::string(astro::to_string(st.error())).c_str());
      ++failures;
      continue;
    }

    bool bad = false;
    for (int i = 0; i < 3; ++i) {
      double rp = 0, rv = 0;
      if (!close_enough(st->position[i], p[i], rp)) bad = true;
      if (!close_enough(st->velocity[i], v[i], rv)) bad = true;
      max_rel = std::fmax(max_rel, std::fmax(rp, rv));
    }
    if (bad) {
      std::fprintf(stderr,
                   "FAIL replay: mismatch u=%d tgt=%d ctr=%d jd=%.9f\n"
                   "  got pos {%.17e,%.17e,%.17e}\n"
                   "  exp pos {%.17e,%.17e,%.17e}\n",
                   units, tgt, ctr, hi + lo, st->position[0], st->position[1],
                   st->position[2], p[0], p[1], p[2]);
      ++failures;
    }
  }

  std::fprintf(stderr, "replay: %ld rows, %ld failures; max relative error %.3e\n",
               rows, failures, max_rel);
  return failures == 0 ? 0 : 1;
}
