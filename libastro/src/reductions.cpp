#include "astro/reductions.hpp"

#include <array>
#include <cmath>

#include "astro/frames.hpp"  // nutation_angles, mean_obliquity

// Layer 2 reductions -- port of NOVAS-C's place() pipeline (research doc 2.2).
// Covers a geocentric or surface observer and the GCRS, astrometric, and
// equator-&-equinox-of-date output systems: ephemeris lookup -> light-time ->
// gravitational deflection -> aberration -> (frame tie + precession + nutation).
// The CIO-based system (coord_sys 2) and the stellar object path remain later
// slices.
//
// NOVAS's own (DE405-derived) constants are used verbatim for oracle
// equivalence; see the note in the earlier revision / research doc Part 3.

namespace astro {

namespace {

// NOVAS constants (novascon.c).
constexpr double kT0 = 2451545.0;
constexpr double kC = 299792458.0;
constexpr double kCAuDay = 173.1446326846693;
constexpr double kAu = 1.4959787069098932e11;
constexpr double kAuKm = 1.4959787069098932e8;
constexpr double kGs = 1.32712440017987e20;
constexpr double kErad = 6378136.6;
constexpr double kFlattening = 0.003352819697896;
constexpr double kAngvel = 7.2921150e-5;
constexpr double kAsec2Rad = 4.848136811095359935899141e-6;
constexpr double kDeg2Rad = 0.017453292519943296;
constexpr double kRad2Deg = 57.295779513082321;
constexpr double kTwoPi = 6.283185307179586476925287;

// Reciprocal masses (Sun/body), NOVAS body number 0..11:
// 0=EMB,1=Mercury,...,3=Earth,5=Jupiter,6=Saturn,...,10=Sun,11=Moon.
constexpr std::array<double, 12> kRmass = {
    328900.561400, 6023600.0,  408523.71,    332946.050895,
    3098708.0,     1047.3486,  3497.898,     22902.98,
    19412.24,      135200000.0, 1.0,         27068700.387534};

double vlen(const double v[3]) {
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}
double dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
double norm_ang(double angle) {
  double a = std::fmod(angle, kTwoPi);
  if (a < 0.0) a += kTwoPi;
  return a;
}

// USNO Circular 179 eq. 2.6: TDB - TT in seconds. NOVAS tdb2tt.
double tdb_minus_tt_seconds(double jd) {
  const double t = (jd - kT0) / 36525.0;
  return 0.001657 * std::sin(628.3076 * t + 6.2401) +
         0.000022 * std::sin(575.3385 * t + 4.2970) +
         0.000014 * std::sin(1256.6152 * t + 6.1969) +
         0.000005 * std::sin(606.9777 * t + 4.0212) +
         0.000005 * std::sin(52.9691 * t + 0.4444) +
         0.000002 * std::sin(21.3299 * t + 5.5431) +
         0.000010 * t * std::sin(628.3076 * t + 4.2490);
}

std::expected<void, EphError> bary_state(const Ephemeris& eph, Point p,
                                         double whole, double frac,
                                         double pos[3], double vel[3]) {
  auto st = eph.state(p, Point::solar_system_barycenter,
                      TdbInstant{JulianDate{whole, frac}}, Units::au);
  if (!st) return std::unexpected(st.error());
  for (int i = 0; i < 3; ++i) {
    pos[i] = st->position[i];
    vel[i] = st->velocity[i];
  }
  return {};
}

void bary2obs(const double pos[3], const double pos_obs[3], double pos2[3],
              double* lighttime) {
  for (int j = 0; j < 3; ++j) pos2[j] = pos[j] - pos_obs[j];
  *lighttime = vlen(pos2) / kCAuDay;
}

double d_light(const double pos1[3], const double pos_obs[3]) {
  const double dis = vlen(pos1);
  const double u1[3] = {pos1[0] / dis, pos1[1] / dis, pos1[2] / dis};
  return dot3(pos_obs, u1) / kCAuDay;
}

std::expected<double, EphError> light_time(const Ephemeris& eph, Point body,
                                           const double pos_obs[3],
                                           double jd_tdb, double tlight0,
                                           bool full, double pos[3]) {
  double tol, jd0, t1, t2;
  if (full) {
    tol = 1.0e-12;
    jd0 = static_cast<double>(static_cast<long>(jd_tdb));
    t1 = jd_tdb - jd0;
    t2 = t1 - tlight0;
  } else {
    tol = 1.0e-9;
    jd0 = 0.0;
    t1 = jd_tdb;
    t2 = jd_tdb - tlight0;
  }
  double t3 = 0.0, tlight = 0.0, pos1[3], vel1[3];
  for (int iter = 0;; ++iter) {
    if (iter > 10) return std::unexpected(EphError::no_convergence);
    if (iter > 0) t2 = t3;
    if (auto r = bary_state(eph, body, jd0, t2, pos1, vel1); !r)
      return std::unexpected(r.error());
    bary2obs(pos1, pos_obs, pos, &tlight);
    t3 = t1 - tlight;
    if (std::fabs(t3 - t2) <= tol) break;
  }
  return tlight;
}

// One-body gravitational deflection (novas.c:grav_vec). pos2 may alias pos1.
void grav_vec(const double pos1[3], const double pos_obs[3],
              const double pos_body[3], double rmass, double pos2[3]) {
  double pq[3], pe[3];
  for (int i = 0; i < 3; ++i) {
    pq[i] = pos_obs[i] + pos1[i] - pos_body[i];
    pe[i] = pos_obs[i] - pos_body[i];
  }
  const double pmag = vlen(pos1), emag = vlen(pe), qmag = vlen(pq);
  double phat[3], ehat[3], qhat[3];
  for (int i = 0; i < 3; ++i) {
    phat[i] = pos1[i] / pmag;
    ehat[i] = pe[i] / emag;
    qhat[i] = pq[i] / qmag;
  }
  const double pdotq = dot3(phat, qhat);
  const double edotp = dot3(ehat, phat);
  const double qdote = dot3(qhat, ehat);
  if (std::fabs(edotp) > 0.99999999999) {
    for (int i = 0; i < 3; ++i) pos2[i] = pos1[i];
    return;
  }
  const double fac1 = 2.0 * kGs / (kC * kC * emag * kAu * rmass);
  const double fac2 = 1.0 + qdote;
  for (int i = 0; i < 3; ++i) {
    const double p2i = phat[i] + fac1 * (pdotq * ehat[i] - edotp * qhat[i]) / fac2;
    pos2[i] = p2i * pmag;
  }
}

// Total gravitational deflection (novas.c:grav_def). loc != 0 adds the Earth
// term (observer off the geocenter). Reduced = Sun; full = Sun+Jupiter+Saturn.
std::expected<void, EphError> grav_def(const Ephemeris& eph, double jd_tdb,
                                       int loc, bool full, const double pos1[3],
                                       const double pos_obs[3], double pos2[3]) {
  struct Gravitator {
    Point point;
    int rmass_index;
  };
  static constexpr std::array<Gravitator, 3> kBodies = {{
      {Point::sun, 10}, {Point::jupiter, 5}, {Point::saturn, 6}}};

  for (int i = 0; i < 3; ++i) pos2[i] = pos1[i];
  const int nbodies = full ? 3 : 1;
  const double tlt = vlen(pos1) / kCAuDay;

  for (int k = 0; k < nbodies; ++k) {
    double pbody[3], vbody[3], pbodyo[3], x;
    if (auto r = bary_state(eph, kBodies[k].point, jd_tdb, 0.0, pbody, vbody); !r)
      return std::unexpected(r.error());
    bary2obs(pbody, pos_obs, pbodyo, &x);
    const double dlt = d_light(pos2, pbodyo);
    double tclose = jd_tdb;
    if (dlt > 0.0) tclose = jd_tdb - dlt;
    if (tlt < dlt) tclose = jd_tdb - tlt;
    if (auto r = bary_state(eph, kBodies[k].point, tclose, 0.0, pbody, vbody); !r)
      return std::unexpected(r.error());
    grav_vec(pos2, pos_obs, pbody,
             kRmass[static_cast<std::size_t>(kBodies[k].rmass_index)], pos2);
  }

  if (loc != 0) {  // Earth deflection
    double pbody[3], vbody[3];
    if (auto r = bary_state(eph, Point::earth, jd_tdb, 0.0, pbody, vbody); !r)
      return std::unexpected(r.error());
    grav_vec(pos2, pos_obs, pbody, kRmass[3], pos2);
  }
  return {};
}

void aberration(const double pos[3], const double ve[3], double lighttime,
                double pos2[3]) {
  double p1mag;
  if (lighttime == 0.0) {
    p1mag = vlen(pos);
    lighttime = p1mag / kCAuDay;
  } else {
    p1mag = lighttime * kCAuDay;
  }
  const double vemag = vlen(ve);
  const double beta = vemag / kCAuDay;
  const double dot = dot3(pos, ve);
  const double cosd = dot / (p1mag * vemag);
  const double gammai = std::sqrt(1.0 - beta * beta);
  const double p = beta * cosd;
  const double q = (1.0 + p / (1.0 + gammai)) * lighttime;
  const double r = 1.0 + p;
  for (int i = 0; i < 3; ++i) pos2[i] = (gammai * pos[i] + q * ve[i]) / r;
}

void vector2radec(const double pos[3], double* ra, double* dec) {
  const double xyproj = std::sqrt(pos[0] * pos[0] + pos[1] * pos[1]);
  if (xyproj == 0.0) {
    *ra = 0.0;
    *dec = (pos[2] == 0.0) ? 0.0 : (pos[2] < 0.0 ? -90.0 : 90.0);
    return;
  }
  *ra = std::atan2(pos[1], pos[0]) / kAsec2Rad / 54000.0;
  *dec = std::atan2(pos[2], xyproj) / kAsec2Rad / 3600.0;
  if (*ra < 0.0) *ra += 24.0;
}

// -------------------- Earth-orientation transforms --------------------------

// ICRS/dynamical frame tie (novas.c:frame_tie). direction < 0: dynamical->ICRS.
void frame_tie(const double pos1[3], int direction, double pos2[3]) {
  constexpr double xi0 = -0.0166170, eta0 = -0.0068192, da0 = -0.01460;
  const double yx = -da0 * kAsec2Rad, zx = xi0 * kAsec2Rad;
  const double xy = da0 * kAsec2Rad, zy = eta0 * kAsec2Rad;
  const double xz = -xi0 * kAsec2Rad, yz = -eta0 * kAsec2Rad;
  const double xx = 1.0 - 0.5 * (yx * yx + zx * zx);
  const double yy = 1.0 - 0.5 * (yx * yx + zy * zy);
  const double zz = 1.0 - 0.5 * (zy * zy + zx * zx);
  if (direction < 0) {
    pos2[0] = xx * pos1[0] + yx * pos1[1] + zx * pos1[2];
    pos2[1] = xy * pos1[0] + yy * pos1[1] + zy * pos1[2];
    pos2[2] = xz * pos1[0] + yz * pos1[1] + zz * pos1[2];
  } else {
    pos2[0] = xx * pos1[0] + xy * pos1[1] + xz * pos1[2];
    pos2[1] = yx * pos1[0] + yy * pos1[1] + yz * pos1[2];
    pos2[2] = zx * pos1[0] + zy * pos1[1] + zz * pos1[2];
  }
}

// Precession between J2000.0 and another epoch (novas.c:precession). Exactly one
// of jd_tdb1/jd_tdb2 must be T0. Matrix per Capitaine et al. (2003).
void precession(double jd_tdb1, const double pos1[3], double jd_tdb2,
                double pos2[3]) {
  double t = (jd_tdb2 - jd_tdb1) / 36525.0;
  if (jd_tdb2 == kT0) t = -t;

  double eps0 = 84381.406;
  double psia = ((((-0.0000000951 * t + 0.000132851) * t - 0.00114045) * t -
                  1.0790069) * t + 5038.481507) * t;
  double omegaa = ((((0.0000003337 * t - 0.000000467) * t - 0.00772503) * t +
                    0.0512623) * t - 0.025754) * t + eps0;
  double chia = ((((-0.0000000560 * t + 0.000170663) * t - 0.00121197) * t -
                  2.3814292) * t + 10.556403) * t;
  eps0 *= kAsec2Rad;
  psia *= kAsec2Rad;
  omegaa *= kAsec2Rad;
  chia *= kAsec2Rad;
  const double sa = std::sin(eps0), ca = std::cos(eps0);
  const double sb = std::sin(-psia), cb = std::cos(-psia);
  const double sc = std::sin(-omegaa), cc = std::cos(-omegaa);
  const double sd = std::sin(chia), cd = std::cos(chia);
  const double xx = cd * cb - sb * sd * cc;
  const double yx = cd * sb * ca + sd * cc * cb * ca - sa * sd * sc;
  const double zx = cd * sb * sa + sd * cc * cb * sa + ca * sd * sc;
  const double xy = -sd * cb - sb * cd * cc;
  const double yy = -sd * sb * ca + cd * cc * cb * ca - sa * cd * sc;
  const double zy = -sd * sb * sa + cd * cc * cb * sa + ca * cd * sc;
  const double xz = sb * sc;
  const double yz = -sc * cb * ca - sa * cc;
  const double zz = -sc * cb * sa + cc * ca;
  if (jd_tdb2 == kT0) {  // epoch -> J2000.0
    pos2[0] = xx * pos1[0] + xy * pos1[1] + xz * pos1[2];
    pos2[1] = yx * pos1[0] + yy * pos1[1] + yz * pos1[2];
    pos2[2] = zx * pos1[0] + zy * pos1[1] + zz * pos1[2];
  } else {  // J2000.0 -> epoch
    pos2[0] = xx * pos1[0] + yx * pos1[1] + zx * pos1[2];
    pos2[1] = xy * pos1[0] + yy * pos1[1] + zy * pos1[2];
    pos2[2] = xz * pos1[0] + yz * pos1[1] + zz * pos1[2];
  }
}

// Complementary terms of the equation of the equinoxes, radians (novas.c:ee_ct).
double ee_ct(double jd_high, double jd_low, Accuracy accuracy) {
  static const short ke0_t[33][14] = {
      {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 2, -2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 2, -2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 2, -2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 2, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 2, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 1, 2, -2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 1, 2, -2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 4, -4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 1, -1, 1, 0, -8, 12, 0, 0, 0, 0, 0, 0},
      {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 0, 2, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 0, 2, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 2, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 1, -2, 2, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 1, -2, 2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 8, -13, 0, 0, 0, 0, 0, -1},
      {0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {2, 0, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 0, 0, -2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 1, 2, -2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 0, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 4, -2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 2, -2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 0, -2, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 0, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
  static const short ke1[14] = {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  static const double se0_t[33][2] = {
      {+2640.96e-6, -0.39e-6}, {+63.52e-6, -0.02e-6}, {+11.75e-6, +0.01e-6},
      {+11.21e-6, +0.01e-6},   {-4.55e-6, +0.00e-6},  {+2.02e-6, +0.00e-6},
      {+1.98e-6, +0.00e-6},    {-1.72e-6, +0.00e-6},  {-1.41e-6, -0.01e-6},
      {-1.26e-6, -0.01e-6},    {-0.63e-6, +0.00e-6},  {-0.63e-6, +0.00e-6},
      {+0.46e-6, +0.00e-6},    {+0.45e-6, +0.00e-6},  {+0.36e-6, +0.00e-6},
      {-0.24e-6, -0.12e-6},    {+0.32e-6, +0.00e-6},  {+0.28e-6, +0.00e-6},
      {+0.27e-6, +0.00e-6},    {+0.26e-6, +0.00e-6},  {-0.21e-6, +0.00e-6},
      {+0.19e-6, +0.00e-6},    {+0.18e-6, +0.00e-6},  {-0.10e-6, +0.05e-6},
      {+0.15e-6, +0.00e-6},    {-0.14e-6, +0.00e-6},  {+0.14e-6, +0.00e-6},
      {-0.14e-6, +0.00e-6},    {+0.14e-6, +0.00e-6},  {+0.13e-6, +0.00e-6},
      {-0.11e-6, +0.00e-6},    {+0.11e-6, +0.00e-6},  {+0.11e-6, +0.00e-6}};
  static const double se1[2] = {-0.87e-6, +0.00e-6};

  const double t = ((jd_high - kT0) + jd_low) / 36525.0;
  double c_terms;

  if (accuracy == Accuracy::full) {
    double fa[14];
    fa[0] = norm_ang((485868.249036 +
                      (715923.2178 + (31.8792 + (0.051635 + (-0.00024470) * t) * t) * t) * t) *
                         kAsec2Rad + std::fmod(1325.0 * t, 1.0) * kTwoPi);
    fa[1] = norm_ang((1287104.793048 +
                      (1292581.0481 + (-0.5532 + (0.000136 + (-0.00001149) * t) * t) * t) * t) *
                         kAsec2Rad + std::fmod(99.0 * t, 1.0) * kTwoPi);
    fa[2] = norm_ang((335779.526232 +
                      (295262.8478 + (-12.7512 + (-0.001037 + (0.00000417) * t) * t) * t) * t) *
                         kAsec2Rad + std::fmod(1342.0 * t, 1.0) * kTwoPi);
    fa[3] = norm_ang((1072260.703692 +
                      (1105601.2090 + (-6.3706 + (0.006593 + (-0.00003169) * t) * t) * t) * t) *
                         kAsec2Rad + std::fmod(1236.0 * t, 1.0) * kTwoPi);
    fa[4] = norm_ang((450160.398036 +
                      (-482890.5431 + (7.4722 + (0.007702 + (-0.00005939) * t) * t) * t) * t) *
                         kAsec2Rad + std::fmod(-5.0 * t, 1.0) * kTwoPi);
    fa[5] = norm_ang(4.402608842 + 2608.7903141574 * t);
    fa[6] = norm_ang(3.176146697 + 1021.3285546211 * t);
    fa[7] = norm_ang(1.753470314 + 628.3075849991 * t);
    fa[8] = norm_ang(6.203480913 + 334.0612426700 * t);
    fa[9] = norm_ang(0.599546497 + 52.9690962641 * t);
    fa[10] = norm_ang(0.874016757 + 21.3299104960 * t);
    fa[11] = norm_ang(5.481293872 + 7.4781598567 * t);
    fa[12] = norm_ang(5.311886287 + 3.8133035638 * t);
    fa[13] = (0.024381750 + 0.00000538691 * t) * t;

    double s0 = 0.0, s1 = 0.0;
    for (int i = 32; i >= 0; --i) {
      double a = 0.0;
      for (int j = 0; j < 14; ++j) a += ke0_t[i][j] * fa[j];
      s0 += se0_t[i][0] * std::sin(a) + se0_t[i][1] * std::cos(a);
    }
    double a = 0.0;
    for (int j = 0; j < 14; ++j) a += ke1[j] * fa[j];
    s1 += se1[0] * std::sin(a) + se1[1] * std::cos(a);
    c_terms = s0 + s1 * t;
  } else {
    double fa2[5];
    fundamental_arguments(t, fa2);
    c_terms = 2640.96e-6 * std::sin(fa2[4]) +
              63.52e-6 * std::sin(2.0 * fa2[4]) +
              11.75e-6 * std::sin(2.0 * fa2[2] - 2.0 * fa2[3] + 3.0 * fa2[4]) +
              11.21e-6 * std::sin(2.0 * fa2[2] - 2.0 * fa2[3] + fa2[4]) -
              4.55e-6 * std::sin(2.0 * fa2[2] - 2.0 * fa2[3] + 2.0 * fa2[4]) +
              2.02e-6 * std::sin(2.0 * fa2[2] + 3.0 * fa2[4]) +
              1.98e-6 * std::sin(2.0 * fa2[2] + fa2[4]) -
              1.72e-6 * std::sin(3.0 * fa2[4]) -
              0.87e-6 * t * std::sin(fa2[4]);
  }
  return c_terms * kAsec2Rad;
}

// Obliquity + nutation angles + equation of the equinoxes (novas.c:e_tilt).
// Outputs: mean/true obliquity (deg), eq. of equinoxes (s), dpsi/deps (arcsec).
void e_tilt(double jd_tdb, Accuracy accuracy, double* mobl, double* tobl,
            double* ee, double* dpsi, double* deps) {
  double dp, de;
  nutation_angles((jd_tdb - kT0) / 36525.0, accuracy, dp, de);  // arcsec
  const double c_terms = ee_ct(jd_tdb, 0.0, accuracy) / kAsec2Rad;  // arcsec
  const double d_psi = dp;  // PSI_COR = 0
  const double d_eps = de;  // EPS_COR = 0
  double mean_ob = mean_obliquity(jd_tdb);  // arcsec
  double true_ob = mean_ob + d_eps;
  mean_ob /= 3600.0;  // degrees
  true_ob /= 3600.0;
  const double eq_eq = (d_psi * std::cos(mean_ob * kDeg2Rad) + c_terms) / 15.0;
  *dpsi = d_psi;
  *deps = d_eps;
  *ee = eq_eq;
  *mobl = mean_ob;
  *tobl = true_ob;
}

// Apply (direction 0) or invert (direction != 0) nutation (novas.c:nutation).
void nutation_rot(double jd_tdb, int direction, Accuracy accuracy,
                  const double pos[3], double pos2[3]) {
  double oblm, oblt, eqeq, psi, eps;
  e_tilt(jd_tdb, accuracy, &oblm, &oblt, &eqeq, &psi, &eps);
  const double cobm = std::cos(oblm * kDeg2Rad), sobm = std::sin(oblm * kDeg2Rad);
  const double cobt = std::cos(oblt * kDeg2Rad), sobt = std::sin(oblt * kDeg2Rad);
  const double cpsi = std::cos(psi * kAsec2Rad), spsi = std::sin(psi * kAsec2Rad);
  const double xx = cpsi;
  const double yx = -spsi * cobm;
  const double zx = -spsi * sobm;
  const double xy = spsi * cobt;
  const double yy = cpsi * cobm * cobt + sobm * sobt;
  const double zy = cpsi * sobm * cobt - cobm * sobt;
  const double xz = spsi * sobt;
  const double yz = cpsi * cobm * sobt - sobm * cobt;
  const double zz = cpsi * sobm * sobt + cobm * cobt;
  if (direction == 0) {
    pos2[0] = xx * pos[0] + yx * pos[1] + zx * pos[2];
    pos2[1] = xy * pos[0] + yy * pos[1] + zy * pos[2];
    pos2[2] = xz * pos[0] + yz * pos[1] + zz * pos[2];
  } else {
    pos2[0] = xx * pos[0] + xy * pos[1] + xz * pos[2];
    pos2[1] = yx * pos[0] + yy * pos[1] + yz * pos[2];
    pos2[2] = zx * pos[0] + zy * pos[1] + zz * pos[2];
  }
}

// Earth Rotation Angle in degrees, UT1 (novas.c:era).
double era_deg(double jd_high, double jd_low) {
  const double thet1 = 0.7790572732640 + 0.00273781191135448 * (jd_high - kT0);
  const double thet2 = 0.00273781191135448 * jd_low;
  const double thet3 = std::fmod(jd_high, 1.0) + std::fmod(jd_low, 1.0);
  double theta = std::fmod(thet1 + thet2 + thet3, 1.0) * 360.0;
  if (theta < 0.0) theta += 360.0;
  return theta;
}

// Greenwich sidereal time (hours), equinox method (novas.c:sidereal_time,
// method=1). `apparent` selects GAST (gst_type=1, adds the equation of the
// equinoxes) over GMST (gst_type=0).
double sidereal_time_hours(double jd_ut1_high, double jd_ut1_low,
                           double delta_t, bool apparent, Accuracy accuracy) {
  const double jd_ut1 = jd_ut1_high + jd_ut1_low;
  const double jd_tt = jd_ut1 + delta_t / 86400.0;
  const double jd_tdb = jd_tt + tdb_minus_tt_seconds(jd_tt) / 86400.0;
  const double t = (jd_tdb - kT0) / 36525.0;
  const double theta = era_deg(jd_ut1_high, jd_ut1_low);
  double eqeq = 0.0;
  if (apparent) {
    double oblm, oblt, ee, psi, eps;
    e_tilt(jd_tdb, accuracy, &oblm, &oblt, &ee, &psi, &eps);
    eqeq = ee * 15.0;  // seconds of time -> arcseconds
  }
  const double st = eqeq + 0.014506 +
                    ((((-0.0000000368 * t - 0.000029956) * t - 0.00000044) * t +
                      1.3915817) * t + 4612.156534) * t;
  double gst = std::fmod(st / 3600.0 + theta, 360.0) / 15.0;
  if (gst < 0.0) gst += 24.0;
  return gst;
}

// Position/velocity of a surface observer wrt Earth's center, true equator &
// equinox of date, AU / AU/day (novas.c:terra). st = local apparent sidereal
// time at the reference meridian, in hours.
void terra(const SurfaceObserver& loc, double st, double pos[3], double vel[3]) {
  const double erad_km = kErad / 1000.0;
  const double df = 1.0 - kFlattening, df2 = df * df;
  const double phi = loc.latitude_deg * kDeg2Rad;
  const double sinphi = std::sin(phi), cosphi = std::cos(phi);
  const double c = 1.0 / std::sqrt(cosphi * cosphi + df2 * sinphi * sinphi);
  const double s = df2 * c;
  const double ht_km = loc.height_m / 1000.0;
  const double ach = erad_km * c + ht_km;
  const double ash = erad_km * s + ht_km;
  const double stlocl = (st * 15.0 + loc.longitude_deg) * kDeg2Rad;
  const double sinst = std::sin(stlocl), cosst = std::cos(stlocl);
  pos[0] = ach * cosphi * cosst;
  pos[1] = ach * cosphi * sinst;
  pos[2] = ash * sinphi;
  vel[0] = -kAngvel * ach * cosphi * sinst;
  vel[1] = kAngvel * ach * cosphi * cosst;
  vel[2] = 0.0;
  for (int j = 0; j < 3; ++j) {
    pos[j] /= kAuKm;
    vel[j] = vel[j] / kAuKm * 86400.0;
  }
}

// Geocentric position/velocity of a surface observer in GCRS, AU / AU/day
// (novas.c:geo_posvel, where == 1).
void geo_posvel_surface(double jd_tt, double delta_t, Accuracy accuracy,
                        const SurfaceObserver& loc, double pos[3],
                        double vel[3]) {
  const double jd_tdb = jd_tt + tdb_minus_tt_seconds(jd_tt) / 86400.0;
  const double jd_ut1 = jd_tt - delta_t / 86400.0;
  const double gmst = sidereal_time_hours(jd_ut1, 0.0, delta_t, false, accuracy);
  double oblm, oblt, eqeq, psi, eps;
  e_tilt(jd_tdb, accuracy, &oblm, &oblt, &eqeq, &psi, &eps);
  const double gast = gmst + eqeq / 3600.0;  // eqeq is in seconds of time

  double pos1[3], vel1[3], pos2[3], vel2[3], pos3[3], vel3[3];
  terra(loc, gast, pos1, vel1);
  nutation_rot(jd_tdb, -1, accuracy, pos1, pos2);
  precession(jd_tdb, pos2, kT0, pos3);
  frame_tie(pos3, -1, pos);
  nutation_rot(jd_tdb, -1, accuracy, vel1, vel2);
  precession(jd_tdb, vel2, kT0, vel3);
  frame_tie(vel3, -1, vel);
}

// Fraction of the Earth-limb nadir angle (novas.c:limb_angle, nadir output).
double limb_nadir_fraction(const double pos_obj[3], const double pos_obs[3]) {
  const double pi = kTwoPi / 2.0;
  const double rade = kErad / kAu;
  const double disobj = vlen(pos_obj), disobs = vlen(pos_obs);
  const double aprad = (disobs >= rade) ? std::asin(rade / disobs) : pi / 2.0;
  const double coszd = dot3(pos_obj, pos_obs) / (disobj * disobs);
  double zdobj;
  if (coszd <= -1.0)
    zdobj = pi;
  else if (coszd >= 1.0)
    zdobj = 0.0;
  else
    zdobj = std::acos(coszd);
  return (pi - zdobj) / aprad;  // nadir angle as fraction of limb radius
}

// Rotate a vector about the z axis by `angle` degrees (novas.c:spin).
void spin(double angle, const double pos1[3], double pos2[3]) {
  const double angr = angle * kDeg2Rad;
  const double c = std::cos(angr), s = std::sin(angr);
  pos2[0] = c * pos1[0] + s * pos1[1];
  pos2[1] = -s * pos1[0] + c * pos1[1];
  pos2[2] = pos1[2];
}

// Polar motion: ITRS -> terrestrial intermediate system (novas.c:wobble,
// direction 0). xp, yp in arcseconds.
void wobble(double tjd, double xp, double yp, const double pos1[3],
            double pos2[3]) {
  const double xpole = xp * kAsec2Rad, ypole = yp * kAsec2Rad;
  const double t = (tjd - kT0) / 36525.0;
  const double tiolon = -(-47.0e-6 * t) * kAsec2Rad;  // -s'
  const double sinx = std::sin(xpole), cosx = std::cos(xpole);
  const double siny = std::sin(ypole), cosy = std::cos(ypole);
  const double sinl = std::sin(tiolon), cosl = std::cos(tiolon);
  const double xx = cosx * cosl;
  const double yx = sinx * siny * cosl + cosy * sinl;
  const double zx = -sinx * cosy * cosl + siny * sinl;
  const double xy = -cosx * sinl;
  const double yy = -sinx * siny * sinl + cosy * cosl;
  const double zy = sinx * cosy * sinl + siny * cosl;
  const double xz = sinx;
  const double yz = -cosx * siny;
  const double zz = cosx * cosy;
  pos2[0] = xx * pos1[0] + yx * pos1[1] + zx * pos1[2];
  pos2[1] = xy * pos1[0] + yy * pos1[1] + zy * pos1[2];
  pos2[2] = xz * pos1[0] + yz * pos1[1] + zz * pos1[2];
}

// Terrestrial -> celestial (equator & equinox of date), equinox method with
// `option == 1` (stop after Earth rotation). novas.c:ter2cel(method=1,option=1).
void ter2cel_equinox(double jd_ut1_high, double jd_ut1_low, double delta_t,
                     Accuracy accuracy, double xp, double yp,
                     const double vec1[3], double vec2[3]) {
  const double jd_ut1 = jd_ut1_high + jd_ut1_low;
  const double jd_tt = jd_ut1 + delta_t / 86400.0;
  const double jd_tdb = jd_tt + tdb_minus_tt_seconds(jd_tt) / 86400.0;

  double v1[3];
  if (xp == 0.0 && yp == 0.0)
    for (int j = 0; j < 3; ++j) v1[j] = vec1[j];
  else
    wobble(jd_tdb, xp, yp, vec1, v1);

  const double gast =
      sidereal_time_hours(jd_ut1_high, jd_ut1_low, delta_t, true, accuracy);
  spin(-gast * 15.0, v1, vec2);  // option == 1: done
}

// Atmospheric refraction in zenith distance, degrees (novas.c:refract).
double refract_zd(const SurfaceObserver& loc, Refraction ref, double zd_obs) {
  if (ref == Refraction::none || zd_obs < 0.1 || zd_obs > 91.0) return 0.0;
  double p, t;
  if (ref == Refraction::from_location) {
    p = loc.pressure_mbar;
    t = loc.temperature_c;
  } else {
    p = 1010.0 * std::exp(-loc.height_m / 9.1e3);
    t = 10.0;
  }
  const double h = 90.0 - zd_obs;
  const double r = 0.016667 / std::tan((h + 7.31 / (h + 4.4)) * kDeg2Rad);
  return r * (0.28 * p / (t + 273.0));
}

// -------------------- place() core ------------------------------------------

std::expected<SkyPos, EphError> place_impl(const Ephemeris& eph, Point body,
                                           double jd_tt, double delta_t,
                                           bool surface,
                                           const SurfaceObserver& loc,
                                           CoordSys sys, Accuracy accuracy) {
  if (sys == CoordSys::equator_cio)
    return std::unexpected(EphError::not_implemented);

  const int n = static_cast<int>(body);
  if (n < 0 || n > 10 || body == Point::earth)
    return std::unexpected(EphError::invalid_argument);

  const bool full = (accuracy == Accuracy::full);
  const double jd_tdb = jd_tt + tdb_minus_tt_seconds(jd_tt) / 86400.0;

  double peb[3], veb[3], psb[3], vsb[3];
  if (auto r = bary_state(eph, Point::earth, jd_tdb, 0.0, peb, veb); !r)
    return std::unexpected(r.error());
  if (auto r = bary_state(eph, Point::sun, jd_tdb, 0.0, psb, vsb); !r)
    return std::unexpected(r.error());
  (void)psb;  // used by rad_vel (not yet ported)

  // Observer geocentric offset.
  double pog[3] = {}, vog[3] = {};
  int locc = 0;
  if (surface) {
    geo_posvel_surface(jd_tt, delta_t, accuracy, loc, pog, vog);
    locc = 1;
  }
  double pob[3], vob[3];
  for (int i = 0; i < 3; ++i) {
    pob[i] = peb[i] + pog[i];
    vob[i] = veb[i] + vog[i];
  }

  double pos1[3], vel1[3];
  if (auto r = bary_state(eph, body, jd_tdb, 0.0, pos1, vel1); !r)
    return std::unexpected(r.error());
  double pos2[3], t_light0;
  bary2obs(pos1, pob, pos2, &t_light0);
  const double dis = t_light0 * kCAuDay;

  double pos3[3];
  auto tl = light_time(eph, body, pob, jd_tdb, t_light0, full, pos3);
  if (!tl) return std::unexpected(tl.error());
  const double t_light = *tl;

  double pos5[3];
  if (sys == CoordSys::astrometric) {
    for (int i = 0; i < 3; ++i) pos5[i] = pos3[i];
  } else {
    if (locc == 1 && limb_nadir_fraction(pos3, pog) < 0.8) locc = 0;
    double pos4[3];
    if (auto r = grav_def(eph, jd_tdb, locc, full, pos3, pob, pos4); !r)
      return std::unexpected(r.error());
    aberration(pos4, vob, t_light, pos5);
  }

  double pos8[3];
  if (sys == CoordSys::equator_equinox) {
    double pos6[3], pos7[3];
    frame_tie(pos5, 1, pos6);
    precession(kT0, pos6, jd_tdb, pos7);
    nutation_rot(jd_tdb, 0, accuracy, pos7, pos8);
  } else {  // gcrs or astrometric: no frame transform
    for (int i = 0; i < 3; ++i) pos8[i] = pos5[i];
  }

  SkyPos out;
  vector2radec(pos8, &out.ra_hours, &out.dec_deg);
  out.distance_au = dis;
  const double x = vlen(pos8);
  for (int i = 0; i < 3; ++i) out.r_hat[i] = pos8[i] / x;
  out.radial_velocity_km_s = 0.0;  // TODO(rad_vel)
  return out;
}

}  // namespace

std::expected<SkyPos, EphError> place(const Ephemeris& eph, Point body,
                                      TtInstant t, DeltaT dt, CoordSys sys,
                                      Accuracy accuracy) {
  (void)dt;  // geocentric observer: delta_t unused
  return place_impl(eph, body, t.jd.value(), 0.0, /*surface=*/false,
                    SurfaceObserver{}, sys, accuracy);
}

std::expected<SkyPos, EphError> place(const Ephemeris& eph, Point body,
                                      TtInstant t, DeltaT dt,
                                      const SurfaceObserver& observer,
                                      CoordSys sys, Accuracy accuracy) {
  return place_impl(eph, body, t.jd.value(), dt.seconds, /*surface=*/true,
                    observer, sys, accuracy);
}

HorizonPos equ2hor(Ut1Instant t, DeltaT dt, Accuracy accuracy, PolarMotion pole,
                   const SurfaceObserver& obs, double ra_hours, double dec_deg,
                   Refraction refraction) {
  const double jd_hi = t.jd.whole, jd_lo = t.jd.frac;
  const double sinlat = std::sin(obs.latitude_deg * kDeg2Rad);
  const double coslat = std::cos(obs.latitude_deg * kDeg2Rad);
  const double sinlon = std::sin(obs.longitude_deg * kDeg2Rad);
  const double coslon = std::cos(obs.longitude_deg * kDeg2Rad);
  const double sindc = std::sin(dec_deg * kDeg2Rad);
  const double cosdc = std::cos(dec_deg * kDeg2Rad);
  const double sinra = std::sin(ra_hours * 15.0 * kDeg2Rad);
  const double cosra = std::cos(ra_hours * 15.0 * kDeg2Rad);

  // Local zenith / north / west basis in the Earth-fixed frame.
  const double uze[3] = {coslat * coslon, coslat * sinlon, sinlat};
  const double une[3] = {-sinlat * coslon, -sinlat * sinlon, coslat};
  const double uwe[3] = {sinlon, -coslon, 0.0};

  // Rotate the basis to the celestial system of date.
  double uz[3], un[3], uw[3];
  ter2cel_equinox(jd_hi, jd_lo, dt.seconds, accuracy, pole.x_arcsec,
                  pole.y_arcsec, uze, uz);
  ter2cel_equinox(jd_hi, jd_lo, dt.seconds, accuracy, pole.x_arcsec,
                  pole.y_arcsec, une, un);
  ter2cel_equinox(jd_hi, jd_lo, dt.seconds, accuracy, pole.x_arcsec,
                  pole.y_arcsec, uwe, uw);

  const double p[3] = {cosdc * cosra, cosdc * sinra, sindc};
  const double pz = dot3(p, uz), pn = dot3(p, un), pw = dot3(p, uw);

  HorizonPos out;
  double proj = std::sqrt(pn * pn + pw * pw);
  double az = 0.0;
  if (proj > 0.0) az = -std::atan2(pw, pn) * kRad2Deg;
  if (az < 0.0) az += 360.0;
  if (az >= 360.0) az -= 360.0;
  double zd = std::atan2(proj, pz) * kRad2Deg;

  out.azimuth_deg = az;
  out.ra_refracted_hours = ra_hours;
  out.dec_refracted_deg = dec_deg;

  if (refraction != Refraction::none) {
    const double zd0 = zd;
    double zd1, refr;
    do {
      zd1 = zd;
      refr = refract_zd(obs, refraction, zd);
      zd = zd0 - refr;
    } while (std::fabs(zd - zd1) > 3.0e-5);

    if (refr > 0.0 && zd > 3.0e-4) {
      const double sinzd = std::sin(zd * kDeg2Rad);
      const double coszd = std::cos(zd * kDeg2Rad);
      const double sinzd0 = std::sin(zd0 * kDeg2Rad);
      const double coszd0 = std::cos(zd0 * kDeg2Rad);
      double pr[3];
      for (int j = 0; j < 3; ++j)
        pr[j] = ((p[j] - coszd0 * uz[j]) / sinzd0) * sinzd + uz[j] * coszd;
      proj = std::sqrt(pr[0] * pr[0] + pr[1] * pr[1]);
      double rar = out.ra_refracted_hours;
      if (proj > 0.0) rar = std::atan2(pr[1], pr[0]) * kRad2Deg / 15.0;
      if (rar < 0.0) rar += 24.0;
      if (rar >= 24.0) rar -= 24.0;
      out.ra_refracted_hours = rar;
      out.dec_refracted_deg = std::atan2(pr[2], proj) * kRad2Deg;
    }
  }
  out.zenith_distance_deg = zd;
  return out;
}

}  // namespace astro
