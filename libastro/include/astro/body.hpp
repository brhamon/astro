#ifndef ASTRO_BODY_HPP
#define ASTRO_BODY_HPP

#include <string_view>

namespace astro {

// Solar-system points addressable in the raw ephemeris layer. The underlying
// values follow NOVAS-C's planet_ephemeris target/center numbering (research
// doc Part 2), so Layer 0 can pass them through without a lookup table:
//
//   0 Mercury  3 Mars     6 Uranus   9  Moon (geocentric)  12 Earth-Moon bary
//   1 Venus    4 Jupiter  7 Neptune  10 Sun
//   2 Earth    5 Saturn   8 Pluto    11 Solar-system bary
//
// Note the ephemeris stores only the Earth-Moon barycenter and a *geocentric*
// Moon; Earth and the barycentric Moon are reconstructed via EMRAT (doc 1.5).
enum class Point : int {
  mercury = 0,
  venus = 1,
  earth = 2,
  mars = 3,
  jupiter = 4,
  saturn = 5,
  uranus = 6,
  neptune = 7,
  pluto = 8,
  moon = 9,
  sun = 10,
  solar_system_barycenter = 11,
  earth_moon_barycenter = 12,
};

constexpr std::string_view to_string(Point p) noexcept {
  switch (p) {
    case Point::mercury:                  return "Mercury";
    case Point::venus:                    return "Venus";
    case Point::earth:                    return "Earth";
    case Point::mars:                     return "Mars";
    case Point::jupiter:                  return "Jupiter";
    case Point::saturn:                   return "Saturn";
    case Point::uranus:                   return "Uranus";
    case Point::neptune:                  return "Neptune";
    case Point::pluto:                    return "Pluto";
    case Point::moon:                     return "Moon";
    case Point::sun:                      return "Sun";
    case Point::solar_system_barycenter:  return "SSB";
    case Point::earth_moon_barycenter:    return "EMB";
  }
  return "?";
}

}  // namespace astro

#endif  // ASTRO_BODY_HPP
