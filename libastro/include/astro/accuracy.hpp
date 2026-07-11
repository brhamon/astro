#ifndef ASTRO_ACCURACY_HPP
#define ASTRO_ACCURACY_HPP

namespace astro {

// NOVAS accuracy selector. `full` uses the complete models (e.g. IAU 2000A
// nutation, Sun+Jupiter+Saturn deflection); `reduced` uses truncated models
// (NU2000K nutation, Sun-only deflection).
enum class Accuracy {
  full = 0,
  reduced = 1,
};

}  // namespace astro

#endif  // ASTRO_ACCURACY_HPP
