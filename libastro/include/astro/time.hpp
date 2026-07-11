#ifndef ASTRO_TIME_HPP
#define ASTRO_TIME_HPP

#include "astro/time_scales.hpp"

// Civil-time utilities: Gregorian calendar <-> Julian date, leap seconds, and
// the UTC -> {TT, UT1, delta_t} derivation the reductions need. The calendar
// routines are ports of NOVAS `julian_date` / `cal_date`; the leap-second
// handling and time-scale algebra follow this project's legacy ephutil model.

namespace astro {

// Julian date of a UT-like instant from a proleptic Gregorian calendar date.
// `hour` is the fractional hour of day [0, 24). Output shares the input's time
// basis (UTC in -> UTC JD out, etc.). NOVAS `julian_date`.
double julian_date(int year, int month, int day, double hour = 0.0);

struct CivilDate {
  int year = 0;
  int month = 0;
  int day = 0;
  double hour = 0.0;
};

// Inverse of julian_date (NOVAS `cal_date`), valid for any jd > 0.
CivilDate calendar_date(double jd);

// Cumulative leap seconds, TAI - UTC in seconds, in effect at UTC Julian date
// `jd_utc`, from the built-in IERS table. The table is current through the last
// entry in src/time.cpp (2017-01-01 = 37 s; no leap seconds announced since).
// Dates before 1972 return the earliest tabulated value.
double tai_minus_utc(double jd_utc);

// Time scales derived from a UTC instant. `ut1_utc` is (UT1 - UTC) in seconds
// from IERS Bulletin A/B; passing 0 is accurate to < ~0.9 s. Relations:
//   TT      = UTC + (TAI - UTC) + 32.184 s
//   UT1     = UTC + (UT1 - UTC)
//   delta_t = TT - UT1 = 32.184 + (TAI - UTC) - (UT1 - UTC)
struct TimeScaleSet {
  double jd_utc = 0.0;
  double leap_seconds = 0.0;  // TAI - UTC
  TtInstant tt{};
  Ut1Instant ut1{};
  DeltaT delta_t{};
};

TimeScaleSet utc_time_scales(double jd_utc, double ut1_utc = 0.0);
TimeScaleSet utc_time_scales(int year, int month, int day, double hour,
                             double ut1_utc = 0.0);

}  // namespace astro

#endif  // ASTRO_TIME_HPP
