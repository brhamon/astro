/*
 * gen_hor.c -- reference generator for equ2hor (apparent RA/Dec -> local
 * zenith distance / azimuth). Links the legacy NOVAS-C archive. equ2hor is a
 * pure Earth-orientation + geometry transform (no ephemeris). test_hor replays.
 *
 * Usage:
 *   gen_hor [out.csv]        (stdout if out.csv omitted)
 *
 * CSV columns:
 *   accuracy,ref,jd_ut1,delta_t,lat,lon,height,temp,pressure,xp,yp,ra,dec,
 *   zd,az,rar,decr
 *     ref : 0 none, 1 standard, 2 from location ; angles deg, ra/rar hours
 */

#include <stdio.h>
#include <stdlib.h>

#include "novas.h"

struct site {
  double lat, lon, height, temp, pressure;
};

int main(int argc, char** argv) {
  if (argc > 2) {
    fprintf(stderr, "usage: %s [out.csv]\n", argv[0]);
    return 2;
  }
  if (argc == 2 && !freopen(argv[1], "w", stdout)) {
    perror(argv[1]);
    return 1;
  }

  const struct site sites[] = {
      {40.0, -105.0, 1650.0, 12.0, 830.0},
      {-31.0, 149.0, 1100.0, 18.0, 900.0}};
  const double jd_ut1s[] = {2451545.0, 2459580.25, 2470000.75};
  const double delta_t = 69.184;
  const double poles[][2] = {{0.0, 0.0}, {0.15, 0.35}};
  const double ras[] = {0.0, 6.0, 12.0, 18.0};
  const double decs[] = {-60.0, -20.0, 20.0, 60.0};

  printf("accuracy,ref,jd_ut1,delta_t,lat,lon,height,temp,pressure,xp,yp,"
         "ra,dec,zd,az,rar,decr\n");

  for (short acc = 0; acc <= 1; ++acc)
    for (short ref = 0; ref <= 2; ++ref)
      for (int s = 0; s < (int)(sizeof(sites) / sizeof(sites[0])); ++s) {
        on_surface loc;
        make_on_surface(sites[s].lat, sites[s].lon, sites[s].height,
                        sites[s].temp, sites[s].pressure, &loc);
        for (int j = 0; j < (int)(sizeof(jd_ut1s) / sizeof(jd_ut1s[0])); ++j)
          for (int p = 0; p < 2; ++p)
            for (int r = 0; r < 4; ++r)
              for (int d = 0; d < 4; ++d) {
                double zd, az, rar, decr;
                equ2hor(jd_ut1s[j], delta_t, acc, poles[p][0], poles[p][1],
                        &loc, ras[r], decs[d], ref, &zd, &az, &rar, &decr);
                printf("%d,%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,"
                       "%.17g,%.17g,%.17g,%.17g,%.17e,%.17e,%.17e,%.17e\n",
                       acc, ref, jd_ut1s[j], delta_t, sites[s].lat,
                       sites[s].lon, sites[s].height, sites[s].temp,
                       sites[s].pressure, poles[p][0], poles[p][1], ras[r],
                       decs[d], zd, az, rar, decr);
              }
      }
  return 0;
}
