// Validate the civil-time utility. Two parts:
//   1. Self-contained checks (leap seconds, delta_t algebra, JD spot values +
//      round-trip) -- always run, no NOVAS needed.
//   2. Calendar <-> Julian-date bit-for-bit vs NOVAS (gen_time) -- when a
//      reference file is supplied.
//
// Usage: test_time [gen_time.csv]

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "astro/time.hpp"

namespace {
int g_failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
      ++g_failures;                                                        \
    }                                                                      \
  } while (0)

bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

void self_checks() {
  // J2000.0 is 2000-01-01 12:00 -> JD 2451545.0, round-trip exact.
  CHECK(astro::julian_date(2000, 1, 1, 12.0) == 2451545.0);
  auto c = astro::calendar_date(2451545.0);
  CHECK(c.year == 2000 && c.month == 1 && c.day == 1);
  CHECK(near(c.hour, 12.0, 1e-9));

  // Modified Julian Date epoch: 1858-11-17 00:00 -> JD 2400000.5.
  CHECK(astro::julian_date(1858, 11, 17, 0.0) == 2400000.5);

  // Cumulative leap seconds (TAI - UTC) at a few UTC instants.
  CHECK(astro::tai_minus_utc(astro::julian_date(2000, 6, 1)) == 32.0);
  CHECK(astro::tai_minus_utc(astro::julian_date(2010, 6, 1)) == 34.0);
  CHECK(astro::tai_minus_utc(astro::julian_date(2020, 1, 1)) == 37.0);

  // delta_t = 32.184 + (TAI-UTC) - (UT1-UTC).
  auto ts = astro::utc_time_scales(2020, 1, 1, 0.0, /*ut1_utc=*/-0.2);
  CHECK(ts.leap_seconds == 37.0);
  CHECK(near(ts.delta_t.seconds, 32.184 + 37.0 - (-0.2), 1e-12));
  // Offsets are kept in the fractional part (two-part JD), so they are
  // recoverable exactly rather than lost to cancellation near JD ~2.46e6.
  CHECK(ts.tt.jd.whole == ts.jd_utc);
  CHECK(near(ts.tt.jd.frac, (37.0 + 32.184) / 86400.0, 1e-18));
  CHECK(ts.ut1.jd.whole == ts.jd_utc);
  CHECK(near(ts.ut1.jd.frac, -0.2 / 86400.0, 1e-18));
}

void novas_checks(const char* csv_path) {
  std::ifstream csv(csv_path);
  if (!csv) {
    std::fprintf(stderr, "SKIP time (calendar): cannot open %s\n", csv_path);
    return;
  }
  std::string line;
  std::getline(csv, line);  // header
  long rows = 0;
  double max_jd = 0, max_hour = 0;
  while (std::getline(csv, line)) {
    if (line.empty()) continue;
    int y, mo, d, cy, cm, cd;
    double hour, jd, chour;
    if (std::sscanf(line.c_str(), "%d,%d,%d,%lf,%lf,%d,%d,%d,%lf", &y, &mo, &d,
                    &hour, &jd, &cy, &cm, &cd, &chour) != 9) {
      std::fprintf(stderr, "FAIL time: malformed row: %s\n", line.c_str());
      ++g_failures;
      continue;
    }
    ++rows;
    const double my_jd = astro::julian_date(y, mo, d, hour);
    CHECK(my_jd == jd);  // exact integer/·arithmetic
    max_jd = std::fmax(max_jd, std::fabs(my_jd - jd));

    const auto c = astro::calendar_date(jd);
    CHECK(c.year == cy && c.month == cm && c.day == cd);
    max_hour = std::fmax(max_hour, std::fabs(c.hour - chour));
    CHECK(near(c.hour, chour, 1e-9));
  }
  std::fprintf(stderr,
               "time (calendar): %ld rows; max |djd|=%.2e max |dhour|=%.2e\n",
               rows, max_jd, max_hour);
}

}  // namespace

int main(int argc, char** argv) {
  self_checks();
  const char* csv = (argc > 1) ? argv[1] : std::getenv("LIBASTRO_TIME");
  if (csv)
    novas_checks(csv);
  else
    std::fprintf(stderr, "note: no calendar reference file; self-checks only\n");

  if (g_failures == 0) {
    std::fprintf(stderr, "time: OK\n");
    return 0;
  }
  std::fprintf(stderr, "time: %d check(s) failed\n", g_failures);
  return 1;
}
