/*
 * gen_ecl.c -- reference generator for the equatorial->ecliptic conversion.
 * Links NOVAS-C and dumps equ2ecl() (coord_sys = 1, true equator & equinox of
 * date; full accuracy) over a grid. Pure function of time + angles, no
 * ephemeris. test_ecl replays it against astro::equ_to_ecl_of_date.
 *
 * Usage: gen_ecl [out.csv]        (stdout if out.csv omitted)
 * CSV columns: jd_tt,ra,dec,elon,elat   (ra hours; dec/elon/elat degrees)
 */

#include <stdio.h>
#include <stdlib.h>

#include "novas.h"

int main(int argc, char** argv) {
  if (argc > 2) { fprintf(stderr, "usage: %s [out.csv]\n", argv[0]); return 2; }
  if (argc == 2 && !freopen(argv[1], "w", stdout)) { perror(argv[1]); return 1; }

  const double jds[] = {2451545.0, 2459580.5, 2470000.5, 2440000.5, 2480000.5};
  const double ras[] = {0.0, 3.5, 6.0, 11.9, 18.0, 23.5};
  const double decs[] = {-80.0, -40.0, -10.0, 0.0, 25.0, 66.0};

  printf("jd_tt,ra,dec,elon,elat\n");
  for (int j = 0; j < (int)(sizeof(jds) / sizeof(jds[0])); ++j)
    for (int r = 0; r < (int)(sizeof(ras) / sizeof(ras[0])); ++r)
      for (int d = 0; d < (int)(sizeof(decs) / sizeof(decs[0])); ++d) {
        double elon, elat;
        equ2ecl(jds[j], 1, 0, ras[r], decs[d], &elon, &elat);
        printf("%.17g,%.17g,%.17g,%.17e,%.17e\n", jds[j], ras[r], decs[d],
               elon, elat);
      }
  return 0;
}
