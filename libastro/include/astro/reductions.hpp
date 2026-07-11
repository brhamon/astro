#ifndef ASTRO_REDUCTIONS_HPP
#define ASTRO_REDUCTIONS_HPP

#include <expected>

#include "astro/accuracy.hpp"
#include "astro/body.hpp"
#include "astro/ephemeris.hpp"
#include "astro/error.hpp"
#include "astro/state_vector.hpp"
#include "astro/time_scales.hpp"

// Layer 2: astrometric reductions, the modern analogue of novas.c. Target is
// full NOVAS parity across the place() pipeline and coordinate transforms
// (research doc Part 3). This header sketches the top of that surface; it will
// grow as Layer 0 stabilises against the test vectors.

namespace astro {

// NOVAS `place` output frames (aliased in the legacy ephutil.h).
enum class CoordSys {
  gcrs = 0,             // GCRS / "local GCRS"
  equator_equinox = 1,  // true equator & equinox of date
  equator_cio = 2,      // true equator & CIO of date
  astrometric = 3,      // no light deflection or aberration
};

// Observer location (research doc: NOVAS `observer` variants). Start with the
// two the sample apps need; near-Earth spacecraft can follow.
struct GeocentricObserver {};

struct SurfaceObserver {
  double latitude_deg = 0.0;   // geodetic (ITRS), north positive
  double longitude_deg = 0.0;  // east positive
  double height_m = 0.0;
  double temperature_c = 10.0;
  double pressure_mbar = 1010.0;
};

// Catalog astrometric data for an object outside the solar system (a star).
// ICRS. Analogue of the astrometric fields of NOVAS `cat_entry`.
struct Star {
  double ra_hours = 0.0;             // ICRS right ascension (hours)
  double dec_deg = 0.0;              // ICRS declination (degrees)
  double pm_ra_mas_yr = 0.0;         // proper motion in RA (milliarcsec/yr)
  double pm_dec_mas_yr = 0.0;        // proper motion in Dec (milliarcsec/yr)
  double parallax_mas = 0.0;         // parallax (milliarcsec); <= 0 => "far away"
  double radial_velocity_km_s = 0.0;
};

// Apparent place of a solar-system body (research doc 2.2). Analogue of
// NOVAS `sky_pos`.
struct SkyPos {
  Vec3 r_hat{};        // unit vector toward object
  double ra_hours = 0.0;
  double dec_deg = 0.0;
  double distance_au = 0.0;
  double radial_velocity_km_s = 0.0;
};

// The hub: reduce a solar-system body's ephemeris state to an on-sky position.
// Analogue of NOVAS `place` (Kaplan et al. 1989; Klioner 2003). The first
// overload places for a geocentric observer (`dt` unused); the second for an
// observer on the Earth's surface.
//
// Implemented so far (validated against NOVAS, see test/unit/test_place.cpp
// and test_star.cpp):
//   - object: major planet, Pluto, Sun, or Moon (not Earth), or a `Star`;
//   - observer: geocenter and Earth surface;
//   - coord_sys: `gcrs` (light deflection + aberration), `astrometric`
//     (neither), and `equator_equinox` (apparent place of date: + frame tie,
//     precession, nutation);
//   - accuracy: `full` (IAU 2000A nutation, Sun+Jupiter+Saturn deflection) and
//     `reduced` (NU2000K nutation, Sun-only deflection);
//   - `SkyPos::radial_velocity_km_s` is computed (rad_vel); `distance_au` is 0
//     for a star.
//
// Pending (return `EphError::not_implemented`): the `equator_cio` CIO system.
std::expected<SkyPos, EphError> place(
    const Ephemeris& eph, Point body, TtInstant t, DeltaT dt,
    CoordSys sys, Accuracy accuracy);

std::expected<SkyPos, EphError> place(
    const Ephemeris& eph, Point body, TtInstant t, DeltaT dt,
    const SurfaceObserver& observer, CoordSys sys, Accuracy accuracy);

std::expected<SkyPos, EphError> place(
    const Ephemeris& eph, const Star& star, TtInstant t, DeltaT dt,
    CoordSys sys, Accuracy accuracy);

std::expected<SkyPos, EphError> place(
    const Ephemeris& eph, const Star& star, TtInstant t, DeltaT dt,
    const SurfaceObserver& observer, CoordSys sys, Accuracy accuracy);

// Polar motion (IERS Bulletin A) in arcseconds; both 0 to ignore.
struct PolarMotion {
  double x_arcsec = 0.0;
  double y_arcsec = 0.0;
};

// Atmospheric refraction model for equ2hor (NOVAS ref_option).
enum class Refraction {
  none = 0,            // no refraction; refracted RA/Dec == input
  standard = 1,        // standard atmosphere from observer height
  from_location = 2,   // use the observer's temperature & pressure
};

// Local horizon coordinates: analogue of NOVAS `sky_pos` for the topocentric
// horizon frame produced by equ2hor.
struct HorizonPos {
  double zenith_distance_deg = 0.0;  // 0 = zenith
  double azimuth_deg = 0.0;          // degrees east of north
  double ra_refracted_hours = 0.0;   // apparent RA shifted for refraction
  double dec_refracted_deg = 0.0;    // apparent Dec shifted for refraction
};

// Convert apparent RA/Dec (true equator & equinox of date, hours/degrees) to
// local zenith distance / azimuth for a surface observer, optionally applying
// atmospheric refraction. Applies polar motion and Earth rotation via the
// equinox-based terrestrial<->celestial transform. Analogue of NOVAS `equ2hor`.
HorizonPos equ2hor(Ut1Instant t, DeltaT dt, Accuracy accuracy, PolarMotion pole,
                   const SurfaceObserver& observer, double ra_hours,
                   double dec_deg, Refraction refraction);

}  // namespace astro

#endif  // ASTRO_REDUCTIONS_HPP
