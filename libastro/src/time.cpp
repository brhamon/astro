#include "astro/time.hpp"

#include <cmath>

namespace astro {

namespace {

// TT - TAI is the fixed offset 32.184 s (by definition).
constexpr double kTtMinusTai = 32.184;

// Cumulative leap seconds (TAI - UTC), keyed by the UTC Julian date on which
// each value took effect. From IERS Bulletin C. Current through 2017-01-01;
// no leap second has been announced since. The trailing sentinel carries the
// latest value forward. (Legacy ephutil.c leap_seconds table.)
struct LeapEntry {
  double jd_utc;
  double tai_utc;
};
constexpr LeapEntry kLeap[] = {
    {2441317.5, 10.0}, {2441499.5, 11.0}, {2441683.5, 12.0}, {2442048.5, 13.0},
    {2442413.5, 14.0}, {2442778.5, 15.0}, {2443144.5, 16.0}, {2443509.5, 17.0},
    {2443874.5, 18.0}, {2444239.5, 19.0}, {2444786.5, 20.0}, {2445151.5, 21.0},
    {2445516.5, 22.0}, {2446247.5, 23.0}, {2447161.5, 24.0}, {2447892.5, 25.0},
    {2448257.5, 26.0}, {2448804.5, 27.0}, {2449169.5, 28.0}, {2449534.5, 29.0},
    {2450083.5, 30.0}, {2450630.5, 31.0}, {2451179.5, 32.0}, {2453736.5, 33.0},
    {2454832.5, 34.0}, {2456109.5, 35.0}, {2457204.5, 36.0}, {2457754.5, 37.0},
    {5373119.5, 37.0}};  // sentinel (far-future, carries latest value)

}  // namespace

double julian_date(int year, int month, int day, double hour) {
  const long y = year, m = month, d = day;
  const long jd12h = d - 32075L +
                     1461L * (y + 4800L + (m - 14L) / 12L) / 4L +
                     367L * (m - 2L - (m - 14L) / 12L * 12L) / 12L -
                     3L * ((y + 4900L + (m - 14L) / 12L) / 100L) / 4L;
  return static_cast<double>(jd12h) - 0.5 + hour / 24.0;
}

CivilDate calendar_date(double jd) {
  const double djd = jd + 0.5;
  const long j = static_cast<long>(djd);

  CivilDate out;
  out.hour = std::fmod(djd, 1.0) * 24.0;

  long k = j + 68569L;
  const long n = 4L * k / 146097L;
  k = k - (146097L * n + 3L) / 4L;
  const long m = 4000L * (k + 1L) / 1461001L;
  k = k - 1461L * m / 4L + 31L;

  long month = 80L * k / 2447L;
  out.day = static_cast<int>(k - 2447L * month / 80L);
  k = month / 11L;
  month = month + 2L - 12L * k;
  out.month = static_cast<int>(month);
  out.year = static_cast<int>(100L * (n - 49L) + m + k);
  return out;
}

double tai_minus_utc(double jd_utc) {
  // Advance while the *next* entry has already taken effect, mirroring the
  // legacy lookup. `kLeap` has a far-future sentinel, so leap[1] is always safe.
  const LeapEntry* leap = kLeap;
  while (leap[1].jd_utc < jd_utc) ++leap;
  return leap->tai_utc;
}

TimeScaleSet utc_time_scales(double jd_utc, double ut1_utc) {
  TimeScaleSet ts;
  ts.jd_utc = jd_utc;
  ts.leap_seconds = tai_minus_utc(jd_utc);
  // Keep the offset in the fractional part rather than folding it into a single
  // double: preserves precision near JD ~2.4e6 (the whole point of the two-part
  // form; research doc 1.4).
  ts.tt = TtInstant{JulianDate{jd_utc, (ts.leap_seconds + kTtMinusTai) / 86400.0}};
  ts.ut1 = Ut1Instant{JulianDate{jd_utc, ut1_utc / 86400.0}};
  ts.delta_t = DeltaT{kTtMinusTai + ts.leap_seconds - ut1_utc};
  return ts;
}

TimeScaleSet utc_time_scales(int year, int month, int day, double hour,
                             double ut1_utc) {
  return utc_time_scales(julian_date(year, month, day, hour), ut1_utc);
}

}  // namespace astro
