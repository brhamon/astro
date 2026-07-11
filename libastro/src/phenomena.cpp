#include "astro/phenomena.hpp"

#include <algorithm>
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
constexpr double kTwoPi = 6.283185307179586476925287;

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

namespace {

// Standard horizon altitude (degrees) for rise/set, per convention.
double h0_for(Horizon h) {
  switch (h) {
    case Horizon::geometric:             return 0.0;
    case Horizon::star:                  return -0.5667;
    case Horizon::sun_upper_limb:        return -0.8333;
    case Horizon::moon:                  return 0.125;
    case Horizon::civil_twilight:        return -6.0;
    case Horizon::nautical_twilight:     return -12.0;
    case Horizon::astronomical_twilight: return -18.0;
  }
  return -0.5667;
}

}  // namespace

std::generator<SkyEvent> horizon_events(const Ephemeris& eph, Point body,
                                        const SurfaceObserver& obs,
                                        TtInstant start, Horizon horizon,
                                        Direction dir, DeltaT dt) {
  constexpr double kPi = 3.14159265358979323846;
  const double h0 = h0_for(horizon);
  const double step = (dir == Direction::forward ? 1.0 : -1.0) * (1.0 / 48.0);

  // Geometric topocentric altitude (degrees) at TT jd t; NaN off the ephemeris.
  auto altitude = [&](double t) -> double {
    auto sky = place(eph, body, TtInstant{JulianDate{t}}, dt, obs,
                     CoordSys::equator_equinox, Accuracy::full);
    if (!sky) return std::numeric_limits<double>::quiet_NaN();
    const Ut1Instant ut1{JulianDate{t - dt.seconds / 86400.0}};
    const auto hor = equ2hor(ut1, dt, Accuracy::full, PolarMotion{}, obs,
                             sky->ra_hours, sky->dec_deg, Refraction::none);
    return 90.0 - hor.zenith_distance_deg;
  };
  // sin(hour angle): a smooth meridian function, zero at upper transit
  // (crossing - -> +) and lower transit (+ -> -). NaN off the ephemeris.
  auto meridian = [&](double t) -> double {
    auto sky = place(eph, body, TtInstant{JulianDate{t}}, dt, obs,
                     CoordSys::equator_equinox, Accuracy::full);
    if (!sky) return std::numeric_limits<double>::quiet_NaN();
    const Ut1Instant ut1{JulianDate{t - dt.seconds / 86400.0}};
    const double gast = greenwich_apparent_sidereal_time(ut1, dt, Accuracy::full);
    const double ha = gast + obs.longitude_deg / 15.0 - sky->ra_hours;  // hours
    return std::sin(ha * kPi / 12.0);
  };
  auto event_at = [&](double t, EventKind k) -> SkyEvent {
    auto sky = place(eph, body, TtInstant{JulianDate{t}}, dt, obs,
                     CoordSys::equator_equinox, Accuracy::full);
    const Ut1Instant ut1{JulianDate{t - dt.seconds / 86400.0}};
    const auto hor = equ2hor(ut1, dt, Accuracy::full, PolarMotion{}, obs,
                             sky ? sky->ra_hours : 0.0, sky ? sky->dec_deg : 0.0,
                             Refraction::none);
    return SkyEvent{k, TtInstant{JulianDate{t}}, 90.0 - hor.zenith_distance_deg,
                    hor.azimuth_deg};
  };

  double t0 = start.jd.value();
  double alt0 = altitude(t0), mer0 = meridian(t0);
  if (std::isnan(alt0) || std::isnan(mer0)) co_return;

  for (;;) {
    const double t1 = t0 + step;
    const double alt1 = altitude(t1), mer1 = meridian(t1);
    if (std::isnan(alt1) || std::isnan(mer1)) co_return;

    // Order the interval endpoints by time so rise/set direction is unambiguous.
    double te, tl, alte, altl, mere, merl;
    if (t0 < t1) { te = t0; tl = t1; alte = alt0; altl = alt1; mere = mer0; merl = mer1; }
    else { te = t1; tl = t0; alte = alt1; altl = alt0; mere = mer1; merl = mer0; }

    double et[2];
    EventKind ek[2];
    int n = 0;
    if ((alte - h0) * (altl - h0) < 0.0) {  // horizon crossing -> rise/set
      et[n] = brent([&](double t) { return altitude(t) - h0; }, te, tl,
                    alte - h0, altl - h0, 1e-7);
      ek[n] = (altl > alte) ? EventKind::rise : EventKind::set;
      ++n;
    }
    if (mere * merl < 0.0) {  // meridian crossing -> transit
      et[n] = brent(meridian, te, tl, mere, merl, 1e-7);
      ek[n] = (merl > mere) ? EventKind::upper_transit : EventKind::lower_transit;
      ++n;
    }
    if (n == 2) {  // emit in the stream's travel direction
      const bool swap = (step > 0.0) ? (et[0] > et[1]) : (et[0] < et[1]);
      if (swap) { std::swap(et[0], et[1]); std::swap(ek[0], ek[1]); }
    }
    for (int i = 0; i < n; ++i) co_yield event_at(et[i], ek[i]);

    t0 = t1; alt0 = alt1; mer0 = mer1;
  }
}

namespace {

// GM of the apsis center, in AU^3/day^2, from the ephemeris constant block.
double gm_center(const Ephemeris& eph, Point center) {
  const auto& c = eph.constants();
  if (center == Point::earth) {
    const auto gmb = c.get("GMB");        // Earth-Moon barycenter GM
    const auto emrat = c.get("EMRAT");    // M_earth / M_moon
    if (gmb && emrat) return *gmb * (*emrat) / (*emrat + 1.0);  // Earth's share
    return 8.887e-10;
  }
  return c.get("GMS").value_or(2.9591220828411951e-4);  // Sun (default)
}

}  // namespace

std::generator<ApsisEvent> apsides(const Ephemeris& eph, Point body,
                                   Point center, TtInstant start,
                                   Direction dir) {
  if (body == center) co_return;
  const double sign = (dir == Direction::forward) ? 1.0 : -1.0;

  // Radial velocity r.v of body relative to center (AU^2/day); apsis at r.v = 0.
  // Optionally reports the distance |r|.
  auto rv = [&](double jd_tdb, double* dist) -> double {
    auto st = eph.state(body, center, TdbInstant{JulianDate{jd_tdb}}, Units::au);
    if (!st) {
      if (dist) *dist = std::numeric_limits<double>::quiet_NaN();
      return std::numeric_limits<double>::quiet_NaN();
    }
    const auto& p = st->position;
    const auto& v = st->velocity;
    if (dist) *dist = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    return p[0] * v[0] + p[1] * v[1] + p[2] * v[2];
  };

  double t = tdb_from_tt(start).jd.value();

  // Estimate the orbital period (vis-viva) to size the march stride.
  auto st0 = eph.state(body, center, TdbInstant{JulianDate{t}}, Units::au);
  if (!st0) co_return;
  const double r = std::sqrt(st0->position[0] * st0->position[0] +
                             st0->position[1] * st0->position[1] +
                             st0->position[2] * st0->position[2]);
  const double v2 = st0->velocity[0] * st0->velocity[0] +
                    st0->velocity[1] * st0->velocity[1] +
                    st0->velocity[2] * st0->velocity[2];
  const double gm = gm_center(eph, center);
  const double inv_a = 2.0 / r - v2 / gm;  // 1 / semi-major axis
  const double period =
      (inv_a > 0.0) ? kTwoPi * std::sqrt(std::pow(1.0 / inv_a, 3.0) / gm)
                    : 3652.5;  // fallback ~10 yr for a non-elliptical fit
  const double stride = sign * period / 12.0;

  double g0 = rv(t, nullptr);
  if (std::isnan(g0)) co_return;
  for (;;) {
    const double t1 = t + stride;
    const double g1 = rv(t1, nullptr);
    if (std::isnan(g1)) co_return;
    if (g0 * g1 < 0.0) {  // radial velocity changed sign -> apsis
      double te, tl, ge, gl;
      if (t < t1) { te = t; tl = t1; ge = g0; gl = g1; }
      else { te = t1; tl = t; ge = g1; gl = g0; }
      const double root =
          brent([&](double x) { return rv(x, nullptr); }, te, tl, ge, gl, 1e-6);
      double dist = 0.0;
      rv(root, &dist);
      // r.v ascending through 0 (- -> +) = periapsis (distance minimum).
      const Apsis kind = (gl > ge) ? Apsis::periapsis : Apsis::apoapsis;
      const double tt_jd = root - tdb_minus_tt_seconds(root) / 86400.0;
      co_yield ApsisEvent{kind, TtInstant{JulianDate{tt_jd}}, dist};
    }
    t = t1;
    g0 = g1;
  }
}

}  // namespace astro
