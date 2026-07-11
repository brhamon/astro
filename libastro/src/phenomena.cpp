#include "astro/phenomena.hpp"

#include <cmath>
#include <limits>

#include "astro/frames.hpp"      // mean_obliquity, nutation_angles
#include "astro/reductions.hpp"  // place
#include "astro/time.hpp"        // tdb_minus_tt_seconds

namespace astro {

namespace {

constexpr double kT0 = 2451545.0;
constexpr double kDeg2Rad = 0.017453292519943296;
constexpr double kRad2Deg = 57.295779513082321;

// Fold an angle difference into (-180, 180] degrees.
double fold180(double x) {
  x = std::fmod(x, 360.0);
  if (x <= -180.0) x += 360.0;
  else if (x > 180.0) x -= 360.0;
  return x;
}

// Root of f in [a,b] (opposite signs at the ends) by Brent's method. Derivative-
// free and robust; f is one place()-based evaluation per call.
template <class F>
double brent(F f, double a, double b, double fa, double fb, double tol) {
  double c = a, fc = fa, d = b - a, e = d;
  for (int it = 0; it < 100; ++it) {
    if (fb * fc > 0.0) { c = a; fc = fa; d = b - a; e = d; }
    if (std::fabs(fc) < std::fabs(fb)) {
      a = b; b = c; c = a;
      fa = fb; fb = fc; fc = fa;
    }
    const double tol1 = 2.0 * 1e-15 * std::fabs(b) + 0.5 * tol;
    const double xm = 0.5 * (c - b);
    if (std::fabs(xm) <= tol1 || fb == 0.0) return b;
    if (std::fabs(e) >= tol1 && std::fabs(fa) > std::fabs(fb)) {
      const double s = fb / fa;
      double p, q, r;
      if (a == c) { p = 2.0 * xm * s; q = 1.0 - s; }
      else {
        q = fa / fc; r = fb / fc;
        p = s * (2.0 * xm * q * (q - r) - (b - a) * (r - 1.0));
        q = (q - 1.0) * (r - 1.0) * (s - 1.0);
      }
      if (p > 0.0) q = -q;
      p = std::fabs(p);
      const double min1 = 3.0 * xm * q - std::fabs(tol1 * q);
      const double min2 = std::fabs(e * q);
      if (2.0 * p < (min1 < min2 ? min1 : min2)) { e = d; d = p / q; }
      else { d = xm; e = d; }
    } else { d = xm; e = d; }
    a = b; fa = fb;
    b += (std::fabs(d) > tol1) ? d : (xm > 0.0 ? tol1 : -tol1);
    fb = f(b);
  }
  return b;
}

}  // namespace

void equ_to_ecl_of_date(double jd_tt, double ra_hours, double dec_deg,
                        double& ecl_lon_deg, double& ecl_lat_deg) {
  const double jd_tdb = jd_tt + tdb_minus_tt_seconds(jd_tt) / 86400.0;
  double dpsi, deps;
  nutation_angles((jd_tdb - kT0) / 36525.0, Accuracy::full, dpsi, deps);
  // True obliquity of date, degrees: mean obliquity + nutation in obliquity.
  const double obl = (mean_obliquity(jd_tdb) + deps) / 3600.0 * kDeg2Rad;

  const double r = ra_hours * 15.0 * kDeg2Rad, d = dec_deg * kDeg2Rad;
  const double p0 = std::cos(d) * std::cos(r);
  const double p1 = std::cos(d) * std::sin(r);
  const double p2 = std::sin(d);
  const double e0 = p0;
  const double e1 = p1 * std::cos(obl) + p2 * std::sin(obl);
  const double e2 = -p1 * std::sin(obl) + p2 * std::cos(obl);

  const double xyproj = std::sqrt(e0 * e0 + e1 * e1);
  ecl_lon_deg = (xyproj > 0.0) ? std::atan2(e1, e0) * kRad2Deg : 0.0;
  if (ecl_lon_deg < 0.0) ecl_lon_deg += 360.0;
  ecl_lat_deg = std::atan2(e2, xyproj) * kRad2Deg;
}

double sun_apparent_longitude(const Ephemeris& eph, TtInstant t) {
  auto sky = place(eph, Point::sun, t, DeltaT{0.0}, CoordSys::equator_equinox,
                   Accuracy::full);
  if (!sky) return std::numeric_limits<double>::quiet_NaN();
  double lon, lat;
  equ_to_ecl_of_date(t.jd.value(), sky->ra_hours, sky->dec_deg, lon, lat);
  return lon;
}

std::generator<SeasonalMoment> tropical_moments(const Ephemeris& eph,
                                                TtInstant start, Direction dir) {
  const double sign = (dir == Direction::forward) ? 1.0 : -1.0;
  const double rate = 360.0 / 365.2422;  // Sun's mean longitude rate, deg/day

  double t0 = start.jd.value();
  double lam0 = sun_apparent_longitude(eph, TtInstant{JulianDate{t0}});
  if (std::isnan(lam0)) co_return;

  // First target: the next multiple of 90 deg in the direction of travel.
  const double k = std::floor(lam0 / 90.0);
  double target = (sign > 0.0) ? (k + 1.0) * 90.0 : k * 90.0;
  if (sign < 0.0 && std::fabs(fold180(lam0 - target)) < 1e-9) target -= 90.0;

  auto g = [&](double t) {
    const double l = sun_apparent_longitude(eph, TtInstant{JulianDate{t}});
    return std::isnan(l) ? l : fold180(l - target);
  };

  for (;;) {
    double guess = t0 + fold180(target - lam0) / rate;  // rate-jump to the root
    double w = 6.0;                                      // days, << quarter year
    double a = guess - w, b = guess + w, ga = g(a), gb = g(b);
    for (int tries = 0; (std::isnan(ga) || std::isnan(gb) || ga * gb > 0.0) &&
                        tries < 8;
         ++tries) {
      if (std::isnan(ga) || std::isnan(gb)) co_return;  // off the ephemeris
      w *= 2.0;
      a = guess - w; b = guess + w; ga = g(a); gb = g(b);
    }
    if (ga * gb > 0.0) co_return;  // failed to bracket

    const double root = brent(g, a, b, ga, gb, 1e-8);

    double tn = std::fmod(target, 360.0);
    if (tn < 0.0) tn += 360.0;
    const int idx = static_cast<int>(std::llround(tn / 90.0)) % 4;
    const Season season = (idx == 0)   ? Season::march_equinox
                          : (idx == 1) ? Season::june_solstice
                          : (idx == 2) ? Season::september_equinox
                                       : Season::december_solstice;
    co_yield SeasonalMoment{season, TtInstant{JulianDate{root}}};

    t0 = root;
    lam0 = tn;  // longitude at the found root == target (normalized)
    target += sign * 90.0;
  }
}

}  // namespace astro
