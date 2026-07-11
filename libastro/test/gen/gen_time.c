/*
 * gen_time.c -- reference generator for the calendar<->Julian-date conversions.
 * Links the legacy NOVAS-C archive and dumps julian_date() forward and
 * cal_date() inverse over a grid of civil dates. Pure arithmetic, no ephemeris.
 * test_time replays these. (Leap seconds / delta_t are this project's own model
 * and are checked directly in test_time, not against NOVAS.)
 *
 * Usage:
 *   gen_time [out.csv]        (stdout if out.csv omitted)
 *
 * CSV columns: year,month,day,hour,jd,cy,cm,cd,chour
 *   (jd = julian_date(year,month,day,hour); c* = cal_date(jd))
 */

#include <stdio.h>
#include <stdlib.h>

#include "novas.h"  /* julian_date, cal_date */

int main(int argc, char** argv) {
  if (argc > 2) {
    fprintf(stderr, "usage: %s [out.csv]\n", argv[0]);
    return 2;
  }
  if (argc == 2 && !freopen(argv[1], "w", stdout)) {
    perror(argv[1]);
    return 1;
  }

  const short years[] = {1700, 1858, 1900, 1972, 2000, 2025, 2200, 2600};
  const short months[] = {1, 2, 6, 12};
  const short days[] = {1, 15, 28};
  const double hours[] = {0.0, 6.5, 18.375};

  printf("year,month,day,hour,jd,cy,cm,cd,chour\n");
  for (int y = 0; y < (int)(sizeof(years) / sizeof(years[0])); ++y)
    for (int m = 0; m < (int)(sizeof(months) / sizeof(months[0])); ++m)
      for (int d = 0; d < (int)(sizeof(days) / sizeof(days[0])); ++d)
        for (int h = 0; h < (int)(sizeof(hours) / sizeof(hours[0])); ++h) {
          double jd = julian_date(years[y], months[m], days[d], hours[h]);
          short cy, cm, cd;
          double chour;
          cal_date(jd, &cy, &cm, &cd, &chour);
          printf("%d,%d,%d,%.17g,%.17g,%d,%d,%d,%.17g\n", years[y], months[m],
                 days[d], hours[h], jd, cy, cm, cd, chour);
        }
  return 0;
}
