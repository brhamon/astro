#ifndef ASTRO_PHENOMENA_HPP
#define ASTRO_PHENOMENA_HPP

#include <generator>

#include "astro/accuracy.hpp"
#include "astro/ephemeris.hpp"
#include "astro/reductions.hpp"   // SurfaceObserver, DeltaT, Point
#include "astro/time_scales.hpp"

// Layer 3: derived phenomena over the place() pipeline -- lazy event streams
// rather than day-keyed tables. Each stream runs forward or backward from a
// start time and is pulled on demand (std::generator), so `std::views::take(n)`
// (or breaking the loop) computes exactly the events you consume.

namespace astro {

enum class Direction { forward, backward };

// Equatorial (true equator & equinox of date) -> ecliptic of date, using the
// true obliquity. Angles in hours (ra) / degrees. Analogue of NOVAS equ2ecl
// with coord_sys = 1; validated bit-for-bit against it.
void equ_to_ecl_of_date(double jd_tt, double ra_hours, double dec_deg,
                        double& ecl_lon_deg, double& ecl_lat_deg);

// The Sun's apparent geocentric ecliptic longitude of date, in [0, 360) degrees.
double sun_apparent_longitude(const Ephemeris& eph, TtInstant t);

// --- Tropical moments (equinoxes and solstices) ----------------------------
// Defined by the Sun's apparent ecliptic longitude reaching a multiple of 90 deg.
enum class Season {
  march_equinox,       // longitude   0 deg
  june_solstice,       // longitude  90 deg
  september_equinox,   // longitude 180 deg
  december_solstice,   // longitude 270 deg
};

struct SeasonalMoment {
  Season season;
  TtInstant time;
};

// Lazy stream of equinoxes/solstices from `start`, forward or backward in time.
std::generator<SeasonalMoment> tropical_moments(
    const Ephemeris& eph, TtInstant start, Direction dir = Direction::forward);

// --- Rise / transit / set --------------------------------------------------
// Meridian and horizon events for a body seen from a surface observer.
enum class EventKind {
  rise,           // altitude ascending through the horizon (h0)
  upper_transit,  // upper culmination (meridian crossing, highest)
  set,            // altitude descending through the horizon (h0)
  lower_transit,  // lower culmination (meridian crossing, lowest)
};

// Standard horizon altitude h0 for rise/set. "Atmosphere" = a choice of h0,
// applied to the geometric altitude (the robust convention; refraction is not
// evaluated at the horizon directly).
enum class Horizon {
  geometric,               // h0 =  0        (airless)
  star,                    // h0 = -0.5667   (standard refraction)
  sun_upper_limb,          // h0 = -0.8333   (refraction - solar semidiameter)
  moon,                    // h0 = +0.125    (refraction - SD + parallax, Meeus)
  civil_twilight,          // h0 = -6        (Sun center)
  nautical_twilight,       // h0 = -12
  astronomical_twilight,   // h0 = -18
};

struct SkyEvent {
  EventKind kind;
  TtInstant time;
  double altitude_deg = 0.0;  // h0 at rise/set; culmination altitude at transit
  double azimuth_deg = 0.0;   // degrees east of north
};

// Lazy stream of rise/transit/set/lower-transit events for `body` at `observer`,
// forward or backward from `start`. Circumpolar or never-rising bodies simply
// yield no rise/set (the transits still occur). Pull only what you consume.
std::generator<SkyEvent> horizon_events(
    const Ephemeris& eph, Point body, const SurfaceObserver& observer,
    TtInstant start, Horizon horizon = Horizon::star,
    Direction dir = Direction::forward, DeltaT dt = {});

// --- Apsides (perihelion/aphelion, perigee/apogee) -------------------------
// Distance extrema of `body` relative to `center`: periapsis (closest) and
// apoapsis (farthest). A planet about the Sun gives perihelion/aphelion; the
// Moon about the Earth gives perigee/apogee. An apsis is exactly where the
// radial velocity r.v = 0 -- read straight from the ephemeris state (position
// and velocity), with no observer reduction. Supported centers: Sun, Earth.
enum class Apsis { periapsis, apoapsis };

struct ApsisEvent {
  Apsis kind;
  TtInstant time;
  double distance_au;
};

std::generator<ApsisEvent> apsides(const Ephemeris& eph, Point body,
                                   Point center, TtInstant start,
                                   Direction dir = Direction::forward);

}  // namespace astro

#endif  // ASTRO_PHENOMENA_HPP
