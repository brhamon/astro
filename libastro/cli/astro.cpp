// astro -- a multi-command command-line tool exercising the libastro surface:
// ephemeris state and constants, place() (planets + a star), equ2hor, and the
// phenomena streams (rise/transit/set, seasons, apsides). Civil time is UTC by
// default; --timezone <IANA name|local|utc> controls both input and output, and
// --json emits machine-readable JSON instead of text (on any command).
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
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "astro/catalog.hpp"
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

std::string catalog_path(const argparse::ArgumentParser& p) {
  if (auto c = p.get<std::string>("--catalog"); !c.empty()) return c;
  if (const char* env = std::getenv("LIBASTRO_CATALOG")) return env;
  return "data/hip2.dat";
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

// Resolve --timezone (an IANA name, or the keywords "local"/"utc") to a chrono
// time zone. Prints an error and returns nullptr if the name is unknown.
const std::chrono::time_zone* resolve_zone(const argparse::ArgumentParser& p) {
  const std::string tz = p.get<std::string>("--timezone");
  const std::string key = lower(tz);
  try {
    if (key == "utc") return std::chrono::locate_zone("Etc/UTC");
    if (key == "local") return std::chrono::current_zone();
    return std::chrono::locate_zone(tz);  // IANA names are case-sensitive
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: unknown timezone '%s' (%s)\n", tz.c_str(), e.what());
    return nullptr;
  }
}

// A civil datetime interpreted in `zone` (or the literal "now") -> UTC time
// scales. On a parse failure, or an impossible/ambiguous local time (DST
// transitions), prints an error and returns nullopt.
std::optional<TimeScaleSet> parse_utc(const std::string& s,
                                      const std::chrono::time_zone* zone) {
  using namespace std::chrono;
  if (lower(s) == "now") {
    const double unix =
        duration<double>(system_clock::now().time_since_epoch()).count();
    return utc_time_scales(2440587.5 + unix / 86400.0);
  }
  int y = 0, mo = 0, d = 0, h = 0, mi = 0;
  double se = 0.0;
  const int n = std::sscanf(s.c_str(), "%d-%d-%d%*[ T]%d:%d:%lf", &y, &mo, &d,
                            &h, &mi, &se);
  if (n < 3) {
    std::fprintf(stderr, "error: cannot parse datetime '%s' "
                         "(want YYYY-MM-DD[THH:MM[:SS]] or 'now')\n", s.c_str());
    return std::nullopt;
  }
  double whole = 0.0;
  const double frac = std::modf(se, &whole);
  try {
    const sys_days sd = year{y} / month{static_cast<unsigned>(mo)} /
                        day{static_cast<unsigned>(d)};
    const local_seconds lt = local_days{sd.time_since_epoch()} + hours{h} +
                             minutes{mi} + seconds{static_cast<long long>(whole)};
    const sys_seconds st = zoned_time{zone, lt}.get_sys_time();
    const double unix = static_cast<double>(st.time_since_epoch().count()) + frac;
    return utc_time_scales(2440587.5 + unix / 86400.0);
  } catch (const nonexistent_local_time& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return std::nullopt;
  } catch (const ambiguous_local_time& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return std::nullopt;
  }
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

// TT Julian date -> a civil timestamp rendered in `zone` (e.g. "... PDT" or
// "... UTC"), to the nearest second (ignores UT1-UTC, good to < 1 s).
std::string format_civil(double tt_jd, const std::chrono::time_zone* zone) {
  using namespace std::chrono;
  const double leap = tai_minus_utc(tt_jd);
  const double utc_jd = tt_jd - (leap + 32.184) / 86400.0;
  const long long unix = std::llround((utc_jd - 2440587.5) * 86400.0);
  return std::format("{:%Y-%m-%d %H:%M:%S %Z}",
                     zoned_time{zone, sys_seconds{seconds{unix}}});
}

// TT Julian date -> an ISO-8601 timestamp with numeric offset in `zone` (e.g.
// "2026-08-06T02:21:29-07:00"), the machine-friendly form used in JSON output.
std::string format_iso(double tt_jd, const std::chrono::time_zone* zone) {
  using namespace std::chrono;
  const double leap = tai_minus_utc(tt_jd);
  const double utc_jd = tt_jd - (leap + 32.184) / 86400.0;
  const long long unix = std::llround((utc_jd - 2440587.5) * 86400.0);
  return std::format("{:%FT%T%Ez}", zoned_time{zone, sys_seconds{seconds{unix}}});
}

// A JSON string literal: double-quoted, with the mandatory characters escaped.
std::string jstr(std::string_view v) {
  std::string o = "\"";
  for (char c : v) {
    switch (c) {
      case '"':  o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\t': o += "\\t"; break;
      case '\r': o += "\\r"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char b[8];
          std::snprintf(b, sizeof b, "\\u%04x", static_cast<unsigned char>(c));
          o += b;
        } else {
          o += c;
        }
    }
  }
  o += '"';
  return o;
}

// ------------------------------- subcommands -------------------------------

int run_info(const argparse::ArgumentParser& a) {
  auto eph = open_ephem(a);
  if (!eph) return 1;
  const auto& h = eph->header();
  if (a.get<bool>("--json")) {
    std::printf("{\"title\":%s,\"de_number\":%d,\"jd_begin\":%.10g,"
                "\"jd_end\":%.10g,\"days_per_record\":%.0f,\"records\":%zu,"
                "\"constants\":%d,\"au_km\":%.10g,\"earth_moon_ratio\":%.12g}\n",
                jstr(h.title).c_str(), h.denum, h.jd_begin, h.jd_end,
                h.days_per_record, h.record_count, h.n_constants, h.au_km,
                h.earth_moon_ratio);
    return 0;
  }
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
  const auto* zone = resolve_zone(a);
  if (!zone) return 2;
  auto ts = parse_utc(a.get<std::string>("datetime"), zone);
  if (!ts) return 2;
  if (a.get<bool>("--json")) {
    std::printf("{\"civil\":%s,\"timezone\":%s,\"jd_utc\":%.6f,"
                "\"leap_seconds\":%.0f,\"jd_tt\":%.9f,\"jd_ut1\":%.9f,"
                "\"jd_tdb\":%.9f,\"delta_t_seconds\":%.3f}\n",
                jstr(format_iso(ts->tt.jd.value(), zone)).c_str(),
                jstr(a.get<std::string>("--timezone")).c_str(), ts->jd_utc,
                ts->leap_seconds, ts->tt.jd.value(), ts->ut1.jd.value(),
                tdb_from_tt(ts->tt).jd.value(), ts->delta_t.seconds);
    return 0;
  }
  std::printf("civil   = %s\n", format_civil(ts->tt.jd.value(), zone).c_str());
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
  const bool json = a.get<bool>("--json");
  const auto names = a.get<std::vector<std::string>>("name");

  if (names.empty()) {  // no names given -> list every constant, sorted by name
    std::vector<std::pair<std::string, double>> all(consts.entries().begin(),
                                                    consts.entries().end());
    std::sort(all.begin(), all.end(),
              [](const auto& x, const auto& y) { return x.first < y.first; });
    if (json) {
      std::printf("{");
      bool first = true;
      for (const auto& [name, value] : all) {
        std::printf("%s%s:%.17g", first ? "" : ",", jstr(name).c_str(), value);
        first = false;
      }
      std::printf("}\n");
      return 0;
    }
    for (const auto& [name, value] : all)
      std::printf("%-10s = %.17g\n", name.c_str(), value);
    std::fprintf(stderr, "(%zu constants; see docs/constants.md)\n", all.size());
    return 0;
  }

  int missing = 0;
  if (json) {
    std::printf("{");
    bool first = true;
    for (const auto& name : names) {
      const auto v = consts.get(name);
      if (!v) ++missing;
      std::printf("%s%s:", first ? "" : ",", jstr(name).c_str());
      if (v) std::printf("%.17g", *v);
      else std::printf("null");
      first = false;
    }
    std::printf("}\n");
    return missing ? 1 : 0;
  }
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
  const auto* zone = resolve_zone(a);
  if (!zone) return 2;
  auto body = parse_body(a.get<std::string>("body"));
  auto center = parse_center(a.get<std::string>("--center"));
  if (!body || !center) { std::fprintf(stderr, "error: bad body/center\n"); return 2; }
  auto ts = parse_utc(a.get<std::string>("datetime"), zone);
  if (!ts) return 2;
  auto st = eph->state(*body, *center, tdb_from_tt(ts->tt), Units::au);
  if (!st) { std::fprintf(stderr, "error: %s\n", std::string(to_string(st.error())).c_str()); return 1; }
  if (a.get<bool>("--json")) {
    std::printf("{\"body\":%s,\"center\":%s,\"jd_tdb\":%.9f,\"units\":\"au\","
                "\"position\":[%.12e,%.12e,%.12e],"
                "\"velocity\":[%.12e,%.12e,%.12e]}\n",
                jstr(to_string(*body)).c_str(), jstr(to_string(*center)).c_str(),
                tdb_from_tt(ts->tt).jd.value(), st->position[0], st->position[1],
                st->position[2], st->velocity[0], st->velocity[1], st->velocity[2]);
    return 0;
  }
  std::printf("position (AU)     : % .12e % .12e % .12e\n",
              st->position[0], st->position[1], st->position[2]);
  std::printf("velocity (AU/day) : % .12e % .12e % .12e\n",
              st->velocity[0], st->velocity[1], st->velocity[2]);
  return 0;
}

int run_place(const argparse::ArgumentParser& a) {
  auto eph = open_ephem(a);
  if (!eph) return 1;
  const auto* zone = resolve_zone(a);
  if (!zone) return 2;
  auto body = parse_body(a.get<std::string>("body"));
  if (!body) { std::fprintf(stderr, "error: bad body\n"); return 2; }
  auto ts = parse_utc(a.get<std::string>("datetime"), zone);
  if (!ts) return 2;
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
  if (a.get<bool>("--json")) {
    std::printf("{\"body\":%s,\"coord\":%s,\"accuracy\":%s,\"ra_hours\":%.9f,"
                "\"dec_deg\":%.9f,\"distance_au\":%.9f,"
                "\"radial_velocity_km_s\":%.6f}\n",
                jstr(to_string(*body)).c_str(),
                jstr(a.get<std::string>("--coord")).c_str(),
                jstr(a.get<std::string>("--accuracy")).c_str(), sky->ra_hours,
                sky->dec_deg, sky->distance_au, sky->radial_velocity_km_s);
    return 0;
  }
  std::printf("RA  = %.6f h\n", sky->ra_hours);
  std::printf("Dec = %.6f deg\n", sky->dec_deg);
  std::printf("dist = %.9f AU\n", sky->distance_au);
  std::printf("rv   = %.4f km/s\n", sky->radial_velocity_km_s);
  return 0;
}

int run_sky(const argparse::ArgumentParser& a) {
  auto eph = open_ephem(a);
  if (!eph) return 1;
  const auto* zone = resolve_zone(a);
  if (!zone) return 2;
  auto ts = parse_utc(a.get<std::string>("datetime"), zone);
  if (!ts) return 2;
  SurfaceObserver obs{47.6096694, -122.340412, 10.0, 15.0, 1026.4};
  if (auto os = a.present<std::string>("--observer")) {
    if (auto o = parse_observer(*os)) obs = *o;
    else { std::fprintf(stderr, "error: bad --observer\n"); return 2; }
  }
  const bool json = a.get<bool>("--json");
  bool first = true;
  auto jrow = [&](std::string_view name, double ra, double dec, double dist,
                  double alt, double az) {
    std::printf("%s{\"object\":%s,\"ra_hours\":%.6f,\"dec_deg\":%.6f,"
                "\"distance_au\":%.9g,\"altitude_deg\":%.4f,\"azimuth_deg\":%.4f}",
                first ? "" : ",", jstr(name).c_str(), ra, dec, dist, alt, az);
    first = false;
  };
  if (json)
    std::printf("{\"observer\":[%.7g,%.7g,%.7g],\"objects\":[",
                obs.latitude_deg, obs.longitude_deg, obs.height_m);
  else
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
    const double alt = 90.0 - hor.zenith_distance_deg;
    if (json)
      jrow(to_string(b), sky->ra_hours, sky->dec_deg, sky->distance_au, alt,
           hor.azimuth_deg);
    else
      std::printf("%-8s %12.6f %13.6f %16.9f %11.4f %11.4f\n",
                  std::string(to_string(b)).c_str(), sky->ra_hours, sky->dec_deg,
                  sky->distance_au, alt, hor.azimuth_deg);
  }
  if (json) std::printf("]}\n");
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
  const auto* zone = resolve_zone(a);
  if (!zone) return 2;
  auto body = parse_body(a.get<std::string>("body"));
  auto obs = parse_observer(a.get<std::string>("--observer"));
  if (!body || !obs) { std::fprintf(stderr, "error: bad body/observer\n"); return 2; }
  auto ts = parse_utc(a.get<std::string>("datetime"), zone);
  if (!ts) return 2;
  const auto dir = a.get<bool>("--back") ? Direction::backward : Direction::forward;
  const bool json = a.get<bool>("--json");
  bool first = true;
  if (json) std::printf("[");
  for (auto e : horizon_events(*eph, *body, *obs, ts->tt, parse_horizon(a.get<std::string>("--horizon")),
                               dir, ts->delta_t) |
                    std::views::take(a.get<int>("--count"))) {
    if (json) {
      std::printf("%s{\"kind\":%s,\"time\":%s,\"jd_tt\":%.9f,"
                  "\"altitude_deg\":%.4f,\"azimuth_deg\":%.4f}", first ? "" : ",",
                  jstr(event_name(e.kind)).c_str(),
                  jstr(format_iso(e.time.jd.value(), zone)).c_str(),
                  e.time.jd.value(), e.altitude_deg, e.azimuth_deg);
      first = false;
    } else {
      std::printf("%-11s %s  alt=%8.4f  az=%9.4f\n", event_name(e.kind),
                  format_civil(e.time.jd.value(), zone).c_str(), e.altitude_deg,
                  e.azimuth_deg);
    }
  }
  if (json) std::printf("]\n");
  return 0;
}

int run_seasons(const argparse::ArgumentParser& a) {
  auto eph = open_ephem(a);
  if (!eph) return 1;
  const auto* zone = resolve_zone(a);
  if (!zone) return 2;
  auto ts = parse_utc(a.get<std::string>("datetime"), zone);
  if (!ts) return 2;
  const auto dir = a.get<bool>("--back") ? Direction::backward : Direction::forward;
  static const char* names[] = {"March equinox", "June solstice",
                                "September equinox", "December solstice"};
  const bool json = a.get<bool>("--json");
  bool first = true;
  if (json) std::printf("[");
  for (auto m : tropical_moments(*eph, ts->tt, dir) |
                    std::views::take(a.get<int>("--count"))) {
    if (json) {
      std::printf("%s{\"season\":%s,\"time\":%s,\"jd_tt\":%.9f}", first ? "" : ",",
                  jstr(names[static_cast<int>(m.season)]).c_str(),
                  jstr(format_iso(m.time.jd.value(), zone)).c_str(),
                  m.time.jd.value());
      first = false;
    } else {
      std::printf("%-18s %s\n", names[static_cast<int>(m.season)],
                  format_civil(m.time.jd.value(), zone).c_str());
    }
  }
  if (json) std::printf("]\n");
  return 0;
}

int run_apsides(const argparse::ArgumentParser& a) {
  auto eph = open_ephem(a);
  if (!eph) return 1;
  const auto* zone = resolve_zone(a);
  if (!zone) return 2;
  auto body = parse_body(a.get<std::string>("body"));
  auto center = parse_center(a.get<std::string>("--center"));
  if (!body || !center) { std::fprintf(stderr, "error: bad body/center\n"); return 2; }
  auto ts = parse_utc(a.get<std::string>("datetime"), zone);
  if (!ts) return 2;
  const auto dir = a.get<bool>("--back") ? Direction::backward : Direction::forward;
  const bool geo = (*center == Point::earth);
  const bool json = a.get<bool>("--json");
  bool first = true;
  if (json) std::printf("[");
  for (auto ap : apsides(*eph, *body, *center, ts->tt, dir) |
                     std::views::take(a.get<int>("--count"))) {
    const char* name = (ap.kind == Apsis::periapsis) ? (geo ? "perigee" : "perihelion")
                                                      : (geo ? "apogee" : "aphelion");
    if (json) {
      std::printf("%s{\"kind\":%s,\"time\":%s,\"jd_tt\":%.9f,\"distance_au\":%.9f}",
                  first ? "" : ",", jstr(name).c_str(),
                  jstr(format_iso(ap.time.jd.value(), zone)).c_str(),
                  ap.time.jd.value(), ap.distance_au);
      first = false;
    } else {
      std::printf("%-10s %s  dist=%.7f AU\n", name,
                  format_civil(ap.time.jd.value(), zone).c_str(), ap.distance_au);
    }
  }
  if (json) std::printf("]\n");
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
  const auto* zone = resolve_zone(a);
  if (!zone) return 2;
  auto ts = parse_utc(a.get<std::string>("datetime"), zone);
  if (!ts) return 2;
  const bool json = a.get<bool>("--json");

  // Stream mode (-n/--back present): principal phase moments, forward or back.
  // Phase moments are geocentric, so --observer does not apply here.
  if (a.is_used("--count") || a.get<bool>("--back")) {
    if (a.is_used("--observer"))
      std::fprintf(stderr, "note: --observer ignored; phase moments are geocentric\n");
    const auto dir = a.get<bool>("--back") ? Direction::backward : Direction::forward;
    static const char* names[] = {"new moon", "first quarter", "full moon",
                                  "last quarter"};
    bool first = true;
    if (json) std::printf("[");
    for (auto m : lunar_phases(*eph, ts->tt, dir) |
                      std::views::take(a.get<int>("--count"))) {
      if (json) {
        std::printf("%s{\"phase\":%s,\"time\":%s,\"jd_tt\":%.9f}", first ? "" : ",",
                    jstr(names[static_cast<int>(m.phase)]).c_str(),
                    jstr(format_iso(m.time.jd.value(), zone)).c_str(),
                    m.time.jd.value());
        first = false;
      } else {
        std::printf("%-14s %s\n", names[static_cast<int>(m.phase)],
                    format_civil(m.time.jd.value(), zone).c_str());
      }
    }
    if (json) std::printf("]\n");
    return 0;
  }

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

  if (json) {
    std::printf("{\"time\":%s,\"mode\":%s,\"phase\":%s,\"illuminated_fraction\":%.6f,"
                "\"phase_angle_deg\":%.4f,\"elongation_deg\":%.4f,\"age_deg\":%.4f,"
                "\"distance_km\":%.3f}\n",
                jstr(format_iso(ts->tt.jd.value(), zone)).c_str(),
                jstr(topo ? "topocentric" : "geocentric").c_str(),
                jstr(moon_phase_name(age)).c_str(), illum, i_deg, elong, age,
                moon_au * eph->header().au_km);
    return 0;
  }
  std::printf("Moon phase @ %s (%s)\n", format_civil(ts->tt.jd.value(), zone).c_str(),
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

int run_star(const argparse::ArgumentParser& a) {
  auto eph = open_ephem(a);
  if (!eph) return 1;
  const auto* zone = resolve_zone(a);
  if (!zone) return 2;
  auto ts = parse_utc(a.get<std::string>("datetime"), zone);
  if (!ts) return 2;

  const std::string ident = a.get<std::string>("id");
  int hip = 0;
  const auto [p, ec] =
      std::from_chars(ident.data(), ident.data() + ident.size(), hip);
  if (ec != std::errc{} || p != ident.data() + ident.size()) {
    // Not a HIP number -> try an IAU proper name (e.g. "Vega").
    if (auto h = hip_for_name(ident)) {
      hip = *h;
    } else {
      std::fprintf(stderr,
                   "error: '%s' is not a HIP number or a known IAU star name\n",
                   ident.c_str());
      return 2;
    }
  }

  const std::string cpath = catalog_path(a);
  auto cat = StarCatalog::load_hipparcos(cpath);
  if (!cat) {
    std::fprintf(stderr, "error: cannot load catalogue '%s': %s\n"
                         "(run scripts/fetch_catalog.sh or set LIBASTRO_CATALOG)\n",
                 cpath.c_str(), std::string(to_string(cat.error())).c_str());
    return 1;
  }
  const CatalogStar* cs = cat->by_hip(hip);
  if (!cs) { std::fprintf(stderr, "error: HIP %d not in catalogue\n", hip); return 1; }

  const CoordSys sys = parse_coord(a.get<std::string>("--coord"));
  std::optional<SurfaceObserver> obs;
  std::expected<SkyPos, EphError> sky;
  if (auto os = a.present<std::string>("--observer")) {
    obs = parse_observer(*os);
    if (!obs) { std::fprintf(stderr, "error: bad --observer\n"); return 2; }
    sky = place(*eph, cs->astrometry, ts->tt, ts->delta_t, *obs, sys, Accuracy::full);
  } else {
    sky = place(*eph, cs->astrometry, ts->tt, ts->delta_t, sys, Accuracy::full);
  }
  if (!sky) { std::fprintf(stderr, "error: %s\n", std::string(to_string(sky.error())).c_str()); return 1; }

  const double dist_ly = parallax_distance_au(cs->astrometry) / 63241.077;  // AU/ly
  double alt = 0.0, az = 0.0;
  const bool have_altaz = obs.has_value();
  if (obs) {
    auto hor = equ2hor(ts->ut1, ts->delta_t, Accuracy::full, PolarMotion{}, *obs,
                       sky->ra_hours, sky->dec_deg, Refraction::from_location);
    alt = 90.0 - hor.zenith_distance_deg;
    az = hor.azimuth_deg;
  }

  const auto iau = name_for_hip(cs->hip);  // canonical IAU proper name, if any
  if (a.get<bool>("--json")) {
    std::printf("{\"hip\":%d", cs->hip);
    if (iau) std::printf(",\"iau_name\":%s", jstr(*iau).c_str());
    std::printf(",\"hp_mag\":");
    if (std::isnan(cs->hp_mag)) std::printf("null"); else std::printf("%.3f", cs->hp_mag);
    std::printf(",\"bv\":");
    if (std::isnan(cs->bv)) std::printf("null"); else std::printf("%.3f", cs->bv);
    std::printf(",\"vi\":");
    if (std::isnan(cs->vi)) std::printf("null"); else std::printf("%.3f", cs->vi);
    std::printf(",\"ra_hours\":%.6f,\"dec_deg\":%.6f,\"distance_ly\":%.4g",
                sky->ra_hours, sky->dec_deg, dist_ly);
    if (have_altaz)
      std::printf(",\"altitude_deg\":%.4f,\"azimuth_deg\":%.4f", alt, az);
    std::printf("}\n");
    return 0;
  }
  std::printf("HIP %d", cs->hip);
  if (iau) std::printf("  IAU %.*s", static_cast<int>(iau->size()), iau->data());
  if (!std::isnan(cs->hp_mag)) std::printf("  Hp=%.2f", cs->hp_mag);
  if (!std::isnan(cs->bv)) std::printf("  B-V=%.2f", cs->bv);
  std::printf("\n");
  std::printf("  RA  = %.6f h\n", sky->ra_hours);
  std::printf("  Dec = %.6f deg\n", sky->dec_deg);
  std::printf("  distance = %.2f ly\n", dist_ly);
  if (have_altaz)
    std::printf("  Alt = %.4f deg  Az = %.4f deg\n", alt, az);
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
  // Timezone for civil-time input and output (default utc = today's behavior).
  auto with_timezone = [](argparse::ArgumentParser& p) {
    p.add_argument("--timezone").default_value(std::string{"utc"})
        .help("civil-time zone for input and output: an IANA name "
              "(e.g. America/Los_Angeles), 'local', or 'utc'");
  };
  auto with_json = [](argparse::ArgumentParser& p) {
    p.add_argument("--json").flag().help("emit machine-readable JSON instead of text");
  };

  argparse::ArgumentParser info("info");
  info.add_description("Show the loaded ephemeris header.");
  with_ephem(info);

  argparse::ArgumentParser time_cmd("time");
  time_cmd.add_description("Convert a civil instant to TT/UT1/TDB and show leap seconds.");
  time_cmd.add_argument("datetime").help("YYYY-MM-DD[THH:MM[:SS]] (in --timezone), or 'now'");
  with_timezone(time_cmd);

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
  with_timezone(state);
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
  with_timezone(place_cmd);
  with_ephem(place_cmd);

  argparse::ArgumentParser star("star");
  star.add_description("Place a Hipparcos catalogue star by HIP number or IAU name.");
  star.add_argument("id").help("Hipparcos number (HIP) or IAU name (e.g. Vega)");
  star.add_argument("datetime").help("UTC or 'now'");
  star.add_argument("--observer").help("lat,lon,height (deg,deg,m); adds alt/az");
  star.add_argument("--coord").default_value(std::string{"apparent"})
      .help("apparent | gcrs | astrometric | cio");
  star.add_argument("--catalog").default_value(std::string{})
      .help("path to hip_main.dat (else $LIBASTRO_CATALOG or data/hip_main.dat)");
  with_timezone(star);
  with_ephem(star);

  argparse::ArgumentParser sky("sky");
  sky.add_description("Planet table (RA/Dec/dist and alt/az) for an observer.");
  sky.add_argument("datetime").help("UTC or 'now'");
  sky.add_argument("--observer").help("lat,lon,height (default: Seattle)");
  with_timezone(sky);
  with_ephem(sky);

  argparse::ArgumentParser rise("rise");
  rise.add_description("Rise / transit / set / antitransit stream for a body.");
  rise.add_argument("body");
  rise.add_argument("datetime").help("UTC start, or 'now'");
  rise.add_argument("--observer").required().help("lat,lon,height");
  rise.add_argument("--horizon").default_value(std::string{"star"})
      .help("geometric|star|sun|moon|civil|nautical|astronomical");
  with_stream(rise);
  with_timezone(rise);
  with_ephem(rise);

  argparse::ArgumentParser seasons("seasons");
  seasons.add_description("Equinox/solstice stream.");
  seasons.add_argument("datetime").help("civil start (in --timezone), or 'now'");
  with_stream(seasons);
  with_timezone(seasons);
  with_ephem(seasons);

  argparse::ArgumentParser apsides_cmd("apsides");
  apsides_cmd.add_description("Perihelion/aphelion (or perigee/apogee) stream.");
  apsides_cmd.add_argument("body");
  apsides_cmd.add_argument("datetime").help("UTC start, or 'now'");
  apsides_cmd.add_argument("--center").default_value(std::string{"sun"})
      .help("sun (perihelion/aphelion) | earth (perigee/apogee)");
  with_stream(apsides_cmd);
  with_timezone(apsides_cmd);
  with_ephem(apsides_cmd);

  argparse::ArgumentParser moon("moon");
  moon.add_description("Moon phase snapshot, or (with -n) a stream of phase moments.");
  moon.add_argument("datetime").help("UTC or 'now'");
  moon.add_argument("--observer")
      .help("lat,lon,height (deg,deg,m); topocentric if given, else geometric geocentric");
  moon.add_argument("-n", "--count").scan<'i', int>().default_value(4)
      .help("stream mode: report this many phase moments (new/quarters/full)");
  moon.add_argument("--back").flag().help("stream mode: search backward in time");
  with_timezone(moon);
  with_ephem(moon);

  for (argparse::ArgumentParser* p :
       {&info, &time_cmd, &constant, &state, &place_cmd, &star, &sky, &rise,
        &seasons, &apsides_cmd, &moon})
    with_json(*p);

  program.add_subparser(info);
  program.add_subparser(time_cmd);
  program.add_subparser(constant);
  program.add_subparser(state);
  program.add_subparser(place_cmd);
  program.add_subparser(star);
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
  if (program.is_subcommand_used("star")) return run_star(star);
  if (program.is_subcommand_used("sky")) return run_sky(sky);
  if (program.is_subcommand_used("rise")) return run_rise(rise);
  if (program.is_subcommand_used("seasons")) return run_seasons(seasons);
  if (program.is_subcommand_used("apsides")) return run_apsides(apsides_cmd);
  if (program.is_subcommand_used("moon")) return run_moon(moon);

  std::cerr << program;
  return 2;
}
