#ifndef ASTRO_STATE_VECTOR_HPP
#define ASTRO_STATE_VECTOR_HPP

#include <array>

namespace astro {

using Vec3 = std::array<double, 3>;

// Units the raw ephemeris state is expressed in. eph_manager's KM flag:
//   au  -> AU and AU/day   (NOVAS default, KM = 0)
//   km  -> km and km/s     (KM = 1)
enum class Units { au, km };

// Barycentric (or center-relative) position/velocity of a solar-system point,
// referenced to the ICRF/J2000 equator & equinox (research doc 1.1, 1.5).
struct StateVector {
  Vec3 position{};  // AU or km
  Vec3 velocity{};  // AU/day or km/s
  Units units = Units::au;
};

}  // namespace astro

#endif  // ASTRO_STATE_VECTOR_HPP
