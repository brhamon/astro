#ifndef ASTRO_PHENOMENA_HPP
#define ASTRO_PHENOMENA_HPP

#include <generator>

#include "astro/accuracy.hpp"
#include "astro/ephemeris.hpp"
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

}  // namespace astro

#endif  // ASTRO_PHENOMENA_HPP
