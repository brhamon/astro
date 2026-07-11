// planets -- a minimal end-to-end demo of libastro, mirroring the legacy
// planets.c: for a surface observer at a civil UTC instant, print each body's
// apparent RA/Dec of date (place, coord_sys = equator & equinox) and its local
// altitude/azimuth (equ2hor). This exercises the whole Layer-0 + Layer-2 stack
// through the public API alone -- no NOVAS.
//
// Usage:
//   planets [YYYY MM DD HH.hhh [ut1_utc_seconds]]
// Time is UTC; TT / UT1 / delta_t are derived via astro::utc_time_scales.
// Ephemeris path from $LIBASTRO_EPHEMERIS (default ./data/JPLEPH).

#include <array>
#include <cstdio>
#include <cstdlib>

#include "astro/ephemeris.hpp"
#include "astro/reductions.hpp"
#include "astro/time.hpp"

int main(int argc, char** argv) {
  const char* eph_path = std::getenv("LIBASTRO_EPHEMERIS");
  const char* path = eph_path ? eph_path : "data/JPLEPH";

  // Default instant: 2025-01-01 00:00 UTC.
  int year = 2025, month = 1, day = 1;
  double hour = 0.0, ut1_utc = 0.0;
  if (argc >= 5) {
    year = std::atoi(argv[1]);
    month = std::atoi(argv[2]);
    day = std::atoi(argv[3]);
    hour = std::strtod(argv[4], nullptr);
  }
  if (argc >= 6) ut1_utc = std::strtod(argv[5], nullptr);

  // Observer: a site near Seattle (matches the legacy demo's default).
  astro::SurfaceObserver obs{47.6096694, -122.340412, 10.0, 15.0, 1026.4};

  auto eph = astro::Ephemeris::open(path);
  if (!eph) {
    std::fprintf(stderr, "cannot open ephemeris %s: %s\n(set LIBASTRO_EPHEMERIS "
                         "or run scripts/fetch_ephemeris.sh)\n",
                 path, std::string(astro::to_string(eph.error())).c_str());
    return 1;
  }

  const astro::TimeScaleSet ts =
      astro::utc_time_scales(year, month, day, hour, ut1_utc);
  const astro::TtInstant tt = ts.tt;
  const astro::Ut1Instant ut1 = ts.ut1;
  const astro::DeltaT delta_t = ts.delta_t;

  std::printf("Ephemeris: %s\n", eph->header().title.c_str());
  std::printf("UTC %04d-%02d-%02d %06.3fh  ->  jd_tt=%.6f  delta_t=%.3f s  "
              "(leap=%.0f s)\n",
              year, month, day, hour, tt.jd.value(), delta_t.seconds,
              ts.leap_seconds);
  std::printf("observer=(%.4f, %.4f)\n\n", obs.latitude_deg, obs.longitude_deg);
  std::printf("%-8s %12s %13s %16s %11s %11s\n", "Object", "RA(h)", "Dec(deg)",
              "Dist(AU)", "Alt(deg)", "Az(deg)");

  constexpr std::array<astro::Point, 10> bodies = {
      astro::Point::mercury, astro::Point::venus,   astro::Point::mars,
      astro::Point::jupiter, astro::Point::saturn,  astro::Point::uranus,
      astro::Point::neptune, astro::Point::pluto,   astro::Point::sun,
      astro::Point::moon};

  for (astro::Point body : bodies) {
    auto sky = astro::place(*eph, body, tt, delta_t, obs,
                            astro::CoordSys::equator_equinox,
                            astro::Accuracy::full);
    if (!sky) {
      std::fprintf(stderr, "%-8s place failed: %s\n",
                   std::string(astro::to_string(body)).c_str(),
                   std::string(astro::to_string(sky.error())).c_str());
      continue;
    }
    auto hor = astro::equ2hor(ut1, delta_t, astro::Accuracy::full,
                              astro::PolarMotion{}, obs, sky->ra_hours,
                              sky->dec_deg, astro::Refraction::from_location);

    std::printf("%-8s %12.6f %13.6f %16.9f %11.4f %11.4f\n",
                std::string(astro::to_string(body)).c_str(), sky->ra_hours,
                sky->dec_deg, sky->distance_au,
                90.0 - hor.zenith_distance_deg, hor.azimuth_deg);
  }

  // A star, exercising the stellar path (proper motion / parallax / rad_vel).
  // Polaris, from the SKY2000 catalog (same figures as the legacy demo).
  const astro::Star polaris{2.5303010278, 89.2641094444,
                            3442.95,       -11.8,
                            7.56,          -17.4};
  if (auto sky = astro::place(*eph, polaris, tt, delta_t, obs,
                              astro::CoordSys::equator_equinox,
                              astro::Accuracy::full)) {
    auto hor = astro::equ2hor(ut1, delta_t, astro::Accuracy::full,
                              astro::PolarMotion{}, obs, sky->ra_hours,
                              sky->dec_deg, astro::Refraction::from_location);
    std::printf("%-8s %12.6f %13.6f %16.9f %11.4f %11.4f  (rv %.2f km/s)\n",
                "Polaris", sky->ra_hours, sky->dec_deg, sky->distance_au,
                90.0 - hor.zenith_distance_deg, hor.azimuth_deg,
                sky->radial_velocity_km_s);
  }
  return 0;
}
