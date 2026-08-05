// astro -- a multi-command command-line tool exercising the libastro surface:
// ephemeris state and constants, place() (planets + a star), equ2hor, and the
// phenomena streams (rise/transit/set, seasons, apsides). Time is UTC.
//
//   astro info
//   astro time    2026-07-11T17:24
//   astro constant AU EMRAT GMS
//   astro state   mars 2026-07-11 [--center ssb|sun|earth]
//   astro place   jupiter 2026-07-11 [--observer 47.6,-122.3,10] [--coord ...]
//   astro sky     2026-07-11 [--observer ...]
//   astro rise    sun  2026-07-11 --observer 47.6,-122.3,10 [--horizon ...] [-n]
//   astro seasons 2026-07-11 [-n N] [--back]
//   astro apsides earth 2026-07-11 [--center sun|earth] [-n N] [--back]
//   astro moon    2026-07-11 [--observer 47.6,-122.3,10]

#include <argparse/argparse.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "astro/ephemeris.hpp"
#include "astro/phenomena.hpp"
#include "astro/reductions.hpp"
#include "astro/time.hpp"

using namespace astro;

namespace {

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string ephem_path(const argparse::ArgumentParser& p) {
  if (auto e = p.get<std::string>("--ephemeris"); !e.empty()) return e;
  if (const char* env = std::getenv("LIBASTRO_EPHEMERIS")) return env;
  return "data/JPLEPH";
}

std::optional<Ephemeris> open_ephem(const argparse::ArgumentParser& p) {
  const std::string path = ephem_path(p);
  auto e = Ephemeris::open(path);
  if (!e) {
    std::fprintf(stderr, "error: cannot open ephemeris '%s': %s\n"
                         "(use --ephemeris PATH, set LIBASTRO_EPHEMERIS, or run "
                         "scripts/fetch_ephemeris.sh)\n",
                 path.c_str(), std::string(to_string(e.error())).c_str());
    return std::nullopt;
  }
  return std::move(*e);
}

// UTC "YYYY-MM-DD[ T]HH:MM[:SS]", or the literal "now" (current system time in
// UTC) -> time scales (with a fixed observer 0 UT1-UTC).
std::optional<TimeScaleSet> parse_utc(const std::string& s) {
  if (lower(s) == "now") {
    const std::time_t t = std::time(nullptr);
    std::tm g{};
    gmtime_r(&t, &g);  // system time -> broken-down UTC
    return utc_time_scales(g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
                           g.tm_hour + g.tm_min / 60.0 + g.tm_sec / 3600.0);
  }
  int y = 0, mo = 0, d = 0, h = 0, mi = 0;
  double se = 0.0;
  const int n = std::sscanf(s.c_str(), "%d-%d-%d%*[ T]%d:%d:%lf", &y, &mo, &d,
                            &h, &mi, &se);
  if (n < 3) return std::nullopt;
  return utc_time_scales(y, mo, d, h + mi / 60.0 + se / 3600.0);
}

std::optional<Point> parse_body(const std::string& name) {
  static const std::pair<const char*, Point> kNames[] = {
      {"mercury", Point::mercury}, {"venus", Point::venus},
      {"earth", Point::earth},     {"mars", Point::mars},
      {"jupiter", Point::jupiter}, {"saturn", Point::saturn},
      {"uranus", Point::uranus},   {"neptune", Point::neptune},
      {"pluto", Point::pluto},     {"moon", Point::moon},
      {"sun", Point::sun}};
  const std::string l = lower(name);
  for (auto& [n, p] : kNames)
    if (l == n) return p;
  return std::nullopt;
}

std::optional<Point> parse_center(const std::string& name) {
  const std::string l = lower(name);
  if (l == "ssb") return Point::solar_system_barycenter;
  if (l == "sun") return Point::sun;
  if (l == "earth") return Point::earth;
  if (l == "emb") return Point::earth_moon_barycenter;
  return std::nullopt;
}

std::optional<SurfaceObserver> parse_observer(const std::string& s) {
  double lat = 0, lon = 0, h = 0;
  if (std::sscanf(s.c_str(), "%lf,%lf,%lf", &lat, &lon, &h) < 2) return std::nullopt;
  return SurfaceObserver{lat, lon, h, 10.0, 1010.0};
}

CoordSys parse_coord(const std::string& s) {
  const std::string l = lower(s);
  if (l == "gcrs") return CoordSys::gcrs;
  if (l == "astrometric") return CoordSys::astrometric;
  if (l == "cio") return CoordSys::equator_cio;
  return CoordSys::equator_equinox;  // "apparent"
}

Accuracy parse_accuracy(const std::string& s) {
  return lower(s) == "reduced" ? Accuracy::reduced : Accuracy::full;
}

Horizon parse_horizon(const std::string& s) {
  const std::string l = lower(s);
  if (l == "geometric") return Horizon::geometric;
  if (l == "sun") return Horizon::sun_upper_limb;
  if (l == "moon") return Horizon::moon;
  if (l == "civil") return Horizon::civil_twilight;
  if (l == "nautical") return Horizon::nautical_twilight;
  if (l == "astronomical") return Horizon::astronomical_twilight;
  return Horizon::star;
}

// TT Julian date -> a printable UTC calendar string (approximate: ignores
// UT1-UTC, good to < 1 s).
std::string tt_to_utc(double tt_jd) {
  const double leap = tai_minus_utc(tt_jd);
  const auto c = calendar_date(tt_jd - (leap + 32.184) / 86400.0);
  const int hh = static_cast<int>(c.hour);
  const int mm = static_cast<int>((c.hour - hh) * 60.0);
  const int ss = static_cast<int>(((c.hour - hh) * 60.0 - mm) * 60.0 + 0.5);
  char buf[40];
  std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02d UTC", c.year,
                c.month, c.day, hh, mm, ss);
  return buf;
}

// ------------------------------- subcommands -------------------------------

int run_info(const argparse::ArgumentParser& a) {
  auto eph = open_ephem(a);
  if (!eph) return 1;
  const auto& h = eph->header();
  std::printf("%s\n", h.title.c_str());
  std::printf("  DE number   : %d\n", h.denum);
  std::printf("  JD span     : %.1f .. %.1f\n", h.jd_begin, h.jd_end);
  std::printf("  days/record : %.0f\n", h.days_per_record);
  std::printf("  records     : %zu\n", h.record_count);
  std::printf("  constants   : %d\n", h.n_constants);
  std::printf("  AU (km)     : %.6f\n", h.au_km);
  std::printf("  Earth/Moon  : %.9f\n", h.earth_moon_ratio);
  return 0;
}

int run_time(const argparse::ArgumentParser& a) {
  auto ts = parse_utc(a.get<std::string>("datetime"));
  if (!ts) { std::fprintf(stderr, "error: bad datetime\n"); return 2; }
  std::printf("UTC  jd = %.6f\n", ts->jd_utc);
  std::printf("leap seconds (TAI-UTC) = %.0f s\n", ts->leap_seconds);
  std::printf("TT   jd = %.9f\n", ts->tt.jd.value());
  std::printf("UT1  jd = %.9f\n", ts->ut1.jd.value());
  std::printf("TDB  jd = %.9f\n", tdb_from_tt(ts->tt).jd.value());
  std::printf("delta_t (TT-UT1) = %.3f s\n", ts->delta_t.seconds);
  return 0;
}

int run_constant(const argparse::ArgumentParser& a) {
  auto eph = open_ephem(a);
  if (!eph) return 1;
  const auto& consts = eph->constants();
  const auto names = a.get<std::vector<std::string>>("name");

  if (names.empty()) {  // no names given -> list every constant, sorted by name
    std::vector<std::pair<std::string, double>> all(consts.entries().begin(),
                                                    consts.entries().end());
    std::sort(all.begin(), all.end(),
              [](const auto& x, const auto& y) { return x.first < y.first; });
    for (const auto& [name, value] : all)
      std::printf("%-10s = %.17g\n", name.c_str(), value);
    std::fprintf(stderr, "(%zu constants; see docs/constants.md)\n", all.size());
    return 0;
  }

  int missing = 0;
  for (const auto& name : names) {
    if (auto v = consts.get(name))
      std::printf("%-10s = %.17g\n", name.c_str(), *v);
    else {
      std::printf("%-10s = (not found)\n", name.c_str());
      ++missing;
    }
  }
  return missing ? 1 : 0;
}

int run_state(const argparse::ArgumentParser& a) {
  auto eph = open_ephem(a);
  if (!eph) return 1;
  auto body = parse_body(a.get<std::string>("body"));
  auto center = parse_center(a.get<std::string>("--center"));
  auto ts = parse_utc(a.get<std::string>("datetime"));
  if (!body || !center || !ts) { std::fprintf(stderr, "error: bad body/center/datetime\n"); return 2; }
  auto st = eph->state(*body, *center, tdb_from_tt(ts->tt), Units::au);
  if (!st) { std::fprintf(stderr, "error: %s\n", std::string(to_string(st.error())).c_str()); return 1; }
  std::printf("position (AU)     : % .12e % .12e % .12e\n",
              st->position[0], st->position[1], st->position[2]);
  std::printf("velocity (AU/day) : % .12e % .12e % .12e\n",
              st->velocity[0], st->velocity[1], st->velocity[2]);
  return 0;
}

int run_place(const argparse::ArgumentParser& a) {
  auto eph = open_ephem(a);
  if (!eph) return 1;
  auto body = parse_body(a.get<std::string>("body"));
  auto ts = parse_utc(a.get<std::string>("datetime"));
  if (!body || !ts) { std::fprintf(stderr, "error: bad body/datetime\n"); return 2; }
  const CoordSys sys = parse_coord(a.get<std::string>("--coord"));
  const Accuracy acc = parse_accuracy(a.get<std::string>("--accuracy"));

  std::expected<SkyPos, EphError> sky;
  if (auto os = a.present<std::string>("--observer")) {
    auto obs = parse_observer(*os);
    if (!obs) { std::fprintf(stderr, "error: bad --observer\n"); return 2; }
    sky = place(*eph, *body, ts->tt, ts->delta_t, *obs, sys, acc);
  } else {
    sky = place(*eph, *body, ts->tt, ts->delta_t, sys, acc);
  }
  if (!sky) { std::fprintf(stderr, "error: %s\n", std::string(to_string(sky.error())).c_str()); return 1; }
  std::printf("RA  = %.6f h\n", sky->ra_hours);
  std::printf("Dec = %.6f deg\n", sky->dec_deg);
  std::printf("dist = %.9f AU\n", sky->distance_au);
  std::printf("rv   = %.4f km/s\n", sky->radial_velocity_km_s);
  return 0;
}

int run_sky(const argparse::ArgumentParser& a) {
  auto eph = open_ephem(a);
  if (!eph) return 1;
  auto ts = parse_utc(a.get<std::string>("datetime"));
  if (!ts) { std::fprintf(stderr, "error: bad datetime\n"); return 2; }
  SurfaceObserver obs{47.6096694, -122.340412, 10.0, 15.0, 1026.4};
  if (auto os = a.present<std::string>("--observer")) {
    if (auto o = parse_observer(*os)) obs = *o;
    else { std::fprintf(stderr, "error: bad --observer\n"); return 2; }
  }
  std::printf("%-8s %12s %13s %16s %11s %11s\n", "Object", "RA(h)", "Dec(deg)",
              "Dist(AU)", "Alt(deg)", "Az(deg)");
  const Point bodies[] = {Point::mercury, Point::venus,  Point::mars,
                          Point::jupiter, Point::saturn, Point::uranus,
                          Point::neptune, Point::pluto,  Point::sun,
                          Point::moon};
  for (Point b : bodies) {
    auto sky = place(*eph, b, ts->tt, ts->delta_t, obs, CoordSys::equator_equinox,
                     Accuracy::full);
    if (!sky) continue;
    auto hor = equ2hor(ts->ut1, ts->delta_t, Accuracy::full, PolarMotion{}, obs,
                       sky->ra_hours, sky->dec_deg, Refraction::from_location);
    std::printf("%-8s %12.6f %13.6f %16.9f %11.4f %11.4f\n",
                std::string(to_string(b)).c_str(), sky->ra_hours, sky->dec_deg,
                sky->distance_au, 90.0 - hor.zenith_distance_deg, hor.azimuth_deg);
  }
  const Star polaris{2.5303010278, 89.2641094444, 3442.95, -11.8, 7.56, -17.4};
  if (auto sky = place(*eph, polaris, ts->tt, ts->delta_t, obs,
                       CoordSys::equator_equinox, Accuracy::full)) {
    auto hor = equ2hor(ts->ut1, ts->delta_t, Accuracy::full, PolarMotion{}, obs,
                       sky->ra_hours, sky->dec_deg, Refraction::from_location);
    std::printf("%-8s %12.6f %13.6f %16.6e %11.4f %11.4f\n", "Polaris",
                sky->ra_hours, sky->dec_deg, parallax_distance_au(polaris),
                90.0 - hor.zenith_distance_deg, hor.azimuth_deg);
  }
  return 0;
}

const char* event_name(EventKind k) {
  switch (k) {
    case EventKind::rise:          return "rise";
    case EventKind::upper_transit: return "transit";
    case EventKind::set:           return "set";
    case EventKind::lower_transit: return "antitransit";
  }
  return "?";
}

int run_rise(const argparse::ArgumentParser& a) {
  auto eph = open_ephem(a);
  if (!eph) return 1;
  auto body = parse_body(a.get<std::string>("body"));
  auto ts = parse_utc(a.get<std::string>("datetime"));
  auto obs = parse_observer(a.get<std::string>("--observer"));
  if (!body || !ts || !obs) { std::fprintf(stderr, "error: bad body/datetime/observer\n"); return 2; }
  const auto dir = a.get<bool>("--back") ? Direction::backward : Direction::forward;
  for (auto e : horizon_events(*eph, *body, *obs, ts->tt, parse_horizon(a.get<std::string>("--horizon")),
                               dir, ts->delta_t) |
                    std::views::take(a.get<int>("--count")))
    std::printf("%-11s %s  alt=%8.4f  az=%9.4f\n", event_name(e.kind),
                tt_to_utc(e.time.jd.value()).c_str(), e.altitude_deg,
                e.azimuth_deg);
  return 0;
}

int run_seasons(const argparse::ArgumentParser& a) {
  auto eph = open_ephem(a);
  if (!eph) return 1;
  auto ts = parse_utc(a.get<std::string>("datetime"));
  if (!ts) { std::fprintf(stderr, "error: bad datetime\n"); return 2; }
  const auto dir = a.get<bool>("--back") ? Direction::backward : Direction::forward;
  static const char* names[] = {"March equinox", "June solstice",
                                "September equinox", "December solstice"};
  for (auto m : tropical_moments(*eph, ts->tt, dir) |
                    std::views::take(a.get<int>("--count")))
    std::printf("%-18s %s\n", names[static_cast<int>(m.season)],
                tt_to_utc(m.time.jd.value()).c_str());
  return 0;
}

int run_apsides(const argparse::ArgumentParser& a) {
  auto eph = open_ephem(a);
  if (!eph) return 1;
  auto body = parse_body(a.get<std::string>("body"));
  auto center = parse_center(a.get<std::string>("--center"));
  auto ts = parse_utc(a.get<std::string>("datetime"));
  if (!body || !center || !ts) { std::fprintf(stderr, "error: bad body/center/datetime\n"); return 2; }
  const auto dir = a.get<bool>("--back") ? Direction::backward : Direction::forward;
  const bool geo = (*center == Point::earth);
  for (auto ap : apsides(*eph, *body, *center, ts->tt, dir) |
                     std::views::take(a.get<int>("--count"))) {
    const char* name = (ap.kind == Apsis::periapsis) ? (geo ? "perigee" : "perihelion")
                                                      : (geo ? "apogee" : "aphelion");
    std::printf("%-10s %s  dist=%.7f AU\n", name,
                tt_to_utc(ap.time.jd.value()).c_str(), ap.distance_au);
  }
  return 0;
}

// --- moon phase ------------------------------------------------------------
constexpr double kRadToDeg = 57.2957795130823208768;
constexpr double kRadToHours = 3.81971863420548805845;  // 12 / pi

// Name one of the eight equal 45-deg phase buckets from the age (Moon - Sun
// ecliptic longitude, 0..360), centered on the cardinal phases.
const char* moon_phase_name(double age_deg) {
  static const char* kNames[8] = {
      "new moon",  "waxing crescent", "first quarter", "waxing gibbous",
      "full moon", "waning gibbous",  "last quarter",  "waning crescent"};
  const int idx = static_cast<int>(std::floor(std::fmod(age_deg + 22.5, 360.0) / 45.0));
  return kNames[((idx % 8) + 8) % 8];
}

int run_moon(const argparse::ArgumentParser& a) {
  auto eph = open_ephem(a);
  if (!eph) return 1;
  auto ts = parse_utc(a.get<std::string>("datetime"));
  if (!ts) { std::fprintf(stderr, "error: bad datetime\n"); return 2; }

  // Moon and Sun vectors (AU) in a common frame, plus apparent RA/Dec for the
  // ecliptic-longitude elongation. Topocentric (apparent) when --observer is
  // given; otherwise geometric geocentric.
  double moon[3], sun[3], ra_m, dec_m, ra_s, dec_s, moon_au;
  bool topo = false;

  if (auto os = a.present<std::string>("--observer")) {
    auto obs = parse_observer(*os);
    if (!obs) { std::fprintf(stderr, "error: bad --observer\n"); return 2; }
    topo = true;
    auto pm = place(*eph, Point::moon, ts->tt, ts->delta_t, *obs,
                    CoordSys::equator_equinox, Accuracy::full);
    auto ps = place(*eph, Point::sun, ts->tt, ts->delta_t, *obs,
                    CoordSys::equator_equinox, Accuracy::full);
    if (!pm || !ps) { std::fprintf(stderr, "error: place failed\n"); return 1; }
    for (int i = 0; i < 3; ++i) {
      moon[i] = pm->distance_au * pm->r_hat[i];
      sun[i] = ps->distance_au * ps->r_hat[i];
    }
    ra_m = pm->ra_hours; dec_m = pm->dec_deg;
    ra_s = ps->ra_hours; dec_s = ps->dec_deg;
    moon_au = pm->distance_au;
  } else {
    const auto tdb = tdb_from_tt(ts->tt);
    auto sm = eph->state(Point::moon, Point::earth, tdb, Units::au);
    auto ss = eph->state(Point::sun, Point::earth, tdb, Units::au);
    if (!sm) { std::fprintf(stderr, "error: %s\n", std::string(to_string(sm.error())).c_str()); return 1; }
    if (!ss) { std::fprintf(stderr, "error: %s\n", std::string(to_string(ss.error())).c_str()); return 1; }
    for (int i = 0; i < 3; ++i) { moon[i] = sm->position[i]; sun[i] = ss->position[i]; }
    const double rm = std::sqrt(moon[0]*moon[0] + moon[1]*moon[1] + moon[2]*moon[2]);
    const double rs = std::sqrt(sun[0]*sun[0] + sun[1]*sun[1] + sun[2]*sun[2]);
    ra_m = std::fmod(std::atan2(moon[1], moon[0]) * kRadToHours + 24.0, 24.0);
    dec_m = std::asin(moon[2] / rm) * kRadToDeg;
    ra_s = std::fmod(std::atan2(sun[1], sun[0]) * kRadToHours + 24.0, 24.0);
    dec_s = std::asin(sun[2] / rs) * kRadToDeg;
    moon_au = rm;
  }

  auto dot = [](const double* p, const double* q) { return p[0]*q[0] + p[1]*q[1] + p[2]*q[2]; };
  const double ms[3] = {moon[0] - sun[0], moon[1] - sun[1], moon[2] - sun[2]};
  const double mag_m = std::sqrt(dot(moon, moon));
  const double mag_s = std::sqrt(dot(sun, sun));
  const double mag_ms = std::sqrt(dot(ms, ms));

  // Phase angle i = angle(Moon, Moon-Sun) = the Sun-Moon-Earth angle (an angle
  // between vectors, so frame-independent). Illuminated fraction = (1+cos i)/2.
  const double cos_i = std::clamp(dot(moon, ms) / (mag_m * mag_ms), -1.0, 1.0);
  const double i_deg = std::acos(cos_i) * kRadToDeg;
  const double illum = 0.5 * (1.0 + cos_i);
  const double elong = std::acos(std::clamp(dot(moon, sun) / (mag_m * mag_s), -1.0, 1.0)) * kRadToDeg;

  // Age = Moon - Sun ecliptic longitude (0..360): drives the named phase and
  // distinguishes waxing from waning (the phase angle alone cannot).
  double lam_m, bet_m, lam_s, bet_s;
  equ_to_ecl_of_date(ts->tt.jd.value(), ra_m, dec_m, lam_m, bet_m);
  equ_to_ecl_of_date(ts->tt.jd.value(), ra_s, dec_s, lam_s, bet_s);
  const double age = std::fmod(lam_m - lam_s + 360.0, 360.0);

  std::printf("Moon phase @ %s (%s)\n", tt_to_utc(ts->tt.jd.value()).c_str(),
              topo ? "topocentric" : "geocentric, geometric");
  std::printf("  phase       : %s\n", moon_phase_name(age));
  std::printf("  illuminated : %.1f%%\n", 100.0 * illum);
  std::printf("  phase angle : %.2f deg  (Sun-Moon-Earth)\n", i_deg);
  std::printf("  elongation  : %.2f deg  (Sun-Moon angular separation)\n", elong);
  std::printf("  age         : %.2f deg  (~%.1f days since new)\n", age,
              age / 360.0 * 29.530588853);
  std::printf("  distance    : %.0f km\n", moon_au * eph->header().au_km);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  argparse::ArgumentParser program("astro", "0.1");
  program.add_description("Sky positions and events from the JPL DE440 ephemeris.");

  auto with_ephem = [](argparse::ArgumentParser& p) {
    p.add_argument("--ephemeris").default_value(std::string{})
        .help("path to the DE ephemeris (else $LIBASTRO_EPHEMERIS or data/JPLEPH)");
  };
  auto with_stream = [](argparse::ArgumentParser& p) {
    p.add_argument("-n", "--count").scan<'i', int>().default_value(6)
        .help("number of events");
    p.add_argument("--back").flag().help("search backward in time");
  };

  argparse::ArgumentParser info("info");
  info.add_description("Show the loaded ephemeris header.");
  with_ephem(info);

  argparse::ArgumentParser time_cmd("time");
  time_cmd.add_description("Convert a UTC instant to TT/UT1/TDB and show leap seconds.");
  time_cmd.add_argument("datetime").help("UTC, YYYY-MM-DD[THH:MM[:SS]], or 'now'");

  argparse::ArgumentParser constant("constant");
  constant.add_description("Look up named ephemeris constants, or list them all.");
  constant.add_argument("name").nargs(argparse::nargs_pattern::any)
      .help("constant name(s); omit to list every constant in the ephemeris");
  with_ephem(constant);

  argparse::ArgumentParser state("state");
  state.add_description("Barycentric/relative state vector of a body.");
  state.add_argument("body");
  state.add_argument("datetime").help("UTC or 'now'");
  state.add_argument("--center").default_value(std::string{"ssb"})
      .help("ssb | sun | earth | emb");
  with_ephem(state);

  argparse::ArgumentParser place_cmd("place");
  place_cmd.add_description("Apparent place (RA/Dec/dist/rv) of a body.");
  place_cmd.add_argument("body");
  place_cmd.add_argument("datetime").help("UTC or 'now'");
  place_cmd.add_argument("--observer").help("lat,lon,height (deg,deg,m); geocentric if omitted");
  place_cmd.add_argument("--coord").default_value(std::string{"apparent"})
      .help("apparent | gcrs | astrometric | cio");
  place_cmd.add_argument("--accuracy").default_value(std::string{"full"})
      .help("full | reduced");
  with_ephem(place_cmd);

  argparse::ArgumentParser sky("sky");
  sky.add_description("Planet+Polaris table (RA/Dec/dist and alt/az) for an observer.");
  sky.add_argument("datetime").help("UTC or 'now'");
  sky.add_argument("--observer").help("lat,lon,height (default: Seattle)");
  with_ephem(sky);

  argparse::ArgumentParser rise("rise");
  rise.add_description("Rise / transit / set / antitransit stream for a body.");
  rise.add_argument("body");
  rise.add_argument("datetime").help("UTC start, or 'now'");
  rise.add_argument("--observer").required().help("lat,lon,height");
  rise.add_argument("--horizon").default_value(std::string{"star"})
      .help("geometric|star|sun|moon|civil|nautical|astronomical");
  with_stream(rise);
  with_ephem(rise);

  argparse::ArgumentParser seasons("seasons");
  seasons.add_description("Equinox/solstice stream.");
  seasons.add_argument("datetime").help("UTC start, or 'now'");
  with_stream(seasons);
  with_ephem(seasons);

  argparse::ArgumentParser apsides_cmd("apsides");
  apsides_cmd.add_description("Perihelion/aphelion (or perigee/apogee) stream.");
  apsides_cmd.add_argument("body");
  apsides_cmd.add_argument("datetime").help("UTC start, or 'now'");
  apsides_cmd.add_argument("--center").default_value(std::string{"sun"})
      .help("sun (perihelion/aphelion) | earth (perigee/apogee)");
  with_stream(apsides_cmd);
  with_ephem(apsides_cmd);

  argparse::ArgumentParser moon("moon");
  moon.add_description("Moon phase: illuminated fraction, phase angle, elongation, named phase.");
  moon.add_argument("datetime").help("UTC or 'now'");
  moon.add_argument("--observer")
      .help("lat,lon,height (deg,deg,m); topocentric if given, else geometric geocentric");
  with_ephem(moon);

  program.add_subparser(info);
  program.add_subparser(time_cmd);
  program.add_subparser(constant);
  program.add_subparser(state);
  program.add_subparser(place_cmd);
  program.add_subparser(sky);
  program.add_subparser(rise);
  program.add_subparser(seasons);
  program.add_subparser(apsides_cmd);
  program.add_subparser(moon);

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    std::cerr << program;
    return 2;
  }

  if (program.is_subcommand_used("info")) return run_info(info);
  if (program.is_subcommand_used("time")) return run_time(time_cmd);
  if (program.is_subcommand_used("constant")) return run_constant(constant);
  if (program.is_subcommand_used("state")) return run_state(state);
  if (program.is_subcommand_used("place")) return run_place(place_cmd);
  if (program.is_subcommand_used("sky")) return run_sky(sky);
  if (program.is_subcommand_used("rise")) return run_rise(rise);
  if (program.is_subcommand_used("seasons")) return run_seasons(seasons);
  if (program.is_subcommand_used("apsides")) return run_apsides(apsides_cmd);
  if (program.is_subcommand_used("moon")) return run_moon(moon);

  std::cerr << program;
  return 2;
}
