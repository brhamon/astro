#ifndef ASTRO_FRAMES_HPP
#define ASTRO_FRAMES_HPP

#include "astro/accuracy.hpp"

// Earth-orientation primitives underpinning the of-date reductions (research
// doc Part 2): the IAU 2000 nutation series, the fundamental (Delaunay)
// arguments it is built on, and the mean obliquity of the ecliptic. Ported from
// NOVAS-C (Kaplan 2005, USNO Circular 179); validated bit-for-bit against it in
// test/unit/test_nutation.cpp.

namespace astro {

// Fundamental (Delaunay) arguments l, l', F, D, Omega, in radians, for
// `t` = Julian centuries of TDB since J2000.0. Simon et al. (1994).
// `a` receives the five arguments. NOVAS `fund_args`.
void fundamental_arguments(double t, double a[5]);

// Mean obliquity of the ecliptic, in arcseconds, at TDB Julian date `jd_tdb`.
// Capitaine et al. (2003). NOVAS `mean_obliq`.
double mean_obliquity(double jd_tdb);

// Nutation in longitude (dpsi) and obliquity (deps), in arcseconds, for
// `t` = Julian centuries of TDB since J2000.0. `full` accuracy uses IAU 2000A;
// `reduced` uses the NU2000K truncation. NOVAS `nutation_angles`.
void nutation_angles(double t, Accuracy accuracy, double& dpsi, double& deps);

}  // namespace astro

#endif  // ASTRO_FRAMES_HPP
