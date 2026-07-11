/*
 * gen_nutation.c -- reference generator for the IAU 2000 nutation series and
 * mean obliquity. Links the legacy NOVAS-C archive and dumps nutation_angles()
 * and mean_obliq() over a grid of epochs and both accuracy modes. These are
 * pure functions of time (no ephemeris needed). test_nutation replays them.
 *
 * Usage:
 *   gen_nutation [out.csv]        (stdout if out.csv omitted)
 *
 * CSV columns: accuracy,jd_tdb,dpsi,deps,mean_obliq   (angles in arcseconds)
 */

#include <stdio.h>
#include <stdlib.h>

#include "novas.h"       /* nutation_angles, mean_obliq */
#include "novascon.h"    /* T0 */

int main(int argc, char** argv) {
  if (argc > 2) {
    fprintf(stderr, "usage: %s [out.csv]\n", argv[0]);
    return 2;
  }
  if (argc == 2 && !freopen(argv[1], "w", stdout)) {
    perror(argv[1]);
    return 1;
  }

  /* Wide epoch grid spanning several centuries either side of J2000. */
  const int n = 250;
  const double jd_lo = 2200000.0;
  const double jd_hi = 2700000.0;
  const double step = (jd_hi - jd_lo) / (double)(n - 1);

  printf("accuracy,jd_tdb,dpsi,deps,mean_obliq\n");
  for (short acc = 0; acc <= 1; ++acc) {
    for (int i = 0; i < n; ++i) {
      const double jd_tdb = jd_lo + step * (double)i;
      const double t = (jd_tdb - T0) / 36525.0;
      double dpsi, deps;
      nutation_angles(t, acc, &dpsi, &deps);
      const double mobl = mean_obliq(jd_tdb);
      printf("%d,%.17g,%.17e,%.17e,%.17e\n", acc, jd_tdb, dpsi, deps, mobl);
    }
  }
  return 0;
}
