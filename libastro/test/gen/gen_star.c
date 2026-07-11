/*
 * gen_star.c -- reference generator for place() on stellar (catalog) objects.
 * Links the legacy NOVAS-C archive and dumps place() outputs for a fixed table
 * of stars across observers, coordinate systems, accuracies, and epochs. The
 * C++ test_star replays these (the star table is duplicated there by index).
 *
 * Usage:
 *   gen_star <path-to-ephemeris> [out.csv]      (stdout if out.csv omitted)
 *
 * CSV columns:
 *   coord_sys,accuracy,star,where,lat,lon,height,delta_t,jd_tt,ra,dec,dis,
 *   rx,ry,rz,rv
 *     star : index into the shared star table below (mirrored in test_star.cpp)
 */

#include <stdio.h>
#include <stdlib.h>

#include "eph_manager.h"
#include "novas.h"

/* Shared star table: ra(h), dec(deg), pmRA(mas/yr), pmDec(mas/yr),
 * parallax(mas), radial velocity(km/s). Star 2 has zero parallax to exercise
 * the "distant object" / zeroed-velocity path. */
struct star_row {
  const char* name;
  double ra, dec, pmra, pmdec, parallax, rv;
};
static const struct star_row stars[] = {
    {"Polaris", 2.5303010278, 89.2641094444, 3442.95, -11.8, 7.56, -17.4},
    {"Barnard", 17.9633, 4.6933, -798.58, 10328.12, 546.98, -110.6},
    {"ZeroPlx", 12.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {"Sirius", 6.7525, -16.7161, -546.0, -1223.0, 379.0, -5.5}};

struct site {
  short where;
  double lat, lon, height, delta_t;
};

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage: %s <ephemeris-file> [out.csv]\n", argv[0]);
    return 2;
  }
  double jd_begin = 0.0, jd_end = 0.0;
  short de_number = 0;
  short err = ephem_open(argv[1], &jd_begin, &jd_end, &de_number);
  if (err != 0) {
    fprintf(stderr, "ephem_open failed (%d) on %s\n", err, argv[1]);
    return 1;
  }
  if (argc == 3 && !freopen(argv[2], "w", stdout)) {
    perror(argv[2]);
    return 1;
  }

  const int n_stars = (int)(sizeof(stars) / sizeof(stars[0]));
  const short coord_systems[] = {0, 1, 3};
  const short accuracies[] = {0, 1};
  const struct site sites[] = {{0, 0.0, 0.0, 0.0, 0.0},
                               {1, 40.0, -105.0, 1650.0, 69.184}};
  const int n_sites = (int)(sizeof(sites) / sizeof(sites[0]));

  const int n_samples = 8;
  const double lo = jd_begin + 1.0, hi = jd_end - 1.0;
  const double step = (hi - lo) / (double)(n_samples + 1);

  printf("coord_sys,accuracy,star,where,lat,lon,height,delta_t,jd_tt,"
         "ra,dec,dis,rx,ry,rz,rv\n");

  long emitted = 0, skipped = 0;
  for (int ci = 0; ci < (int)(sizeof(coord_systems) / sizeof(short)); ++ci)
    for (int ai = 0; ai < 2; ++ai)
      for (int si = 0; si < n_sites; ++si) {
        observer obs;
        if (sites[si].where == 0)
          make_observer_at_geocenter(&obs);
        else
          make_observer_on_surface(sites[si].lat, sites[si].lon,
                                   sites[si].height, 10.0, 1010.0, &obs);
        for (int st = 0; st < n_stars; ++st) {
          cat_entry cat;
          make_cat_entry((char*)stars[st].name, (char*)"REF", st + 1,
                         stars[st].ra, stars[st].dec, stars[st].pmra,
                         stars[st].pmdec, stars[st].parallax, stars[st].rv,
                         &cat);
          object obj;
          if (make_object(2, 0, (char*)stars[st].name, &cat, &obj) != 0)
            continue;
          for (int ti = 1; ti <= n_samples; ++ti) {
            double jd_tt = lo + step * (double)ti;
            sky_pos sp;
            short e = place(jd_tt, &obj, &obs, sites[si].delta_t,
                            coord_systems[ci], accuracies[ai], &sp);
            if (e != 0) {
              ++skipped;
              continue;
            }
            printf("%d,%d,%d,%d,%.17g,%.17g,%.17g,%.17g,%.17g,"
                   "%.17e,%.17e,%.17e,%.17e,%.17e,%.17e,%.17e\n",
                   coord_systems[ci], accuracies[ai], st, sites[si].where,
                   sites[si].lat, sites[si].lon, sites[si].height,
                   sites[si].delta_t, jd_tt, sp.ra, sp.dec, sp.dis,
                   sp.r_hat[0], sp.r_hat[1], sp.r_hat[2], sp.rv);
            ++emitted;
          }
        }
      }

  fprintf(stderr, "emitted %ld star rows, %ld skipped\n", emitted, skipped);
  ephem_close();
  return 0;
}
