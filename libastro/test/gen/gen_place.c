/*
 * gen_place.c -- reference generator for the Layer-2 place() pipeline.
 *
 * Links the legacy NOVAS-C archive and dumps place() outputs across a grid of
 * (coord_sys, accuracy, body, observer, epoch). The C++ test_place replays
 * these; same algorithm + constants => agreement to round-off (research doc 3.5).
 *
 * Coordinate systems emitted are those libastro implements:
 *   0 = GCRS, 1 = equator & equinox of date (apparent), 2 = equator & CIO of
 *   date, 3 = astrometric. Observers: geocenter and two surface sites.
 *
 * Usage:
 *   gen_place <path-to-ephemeris> [out.csv]      (stdout if out.csv omitted)
 *
 * CSV columns:
 *   coord_sys,accuracy,body,where,lat,lon,height,delta_t,jd_tt,ra,dec,dis,rx,ry,rz
 *     where : 0 = geocenter, 1 = surface ; lat/lon deg, height m ; delta_t s
 *     body  : NOVAS object number (1=Mercury..9=Pluto, 10=Sun, 11=Moon)
 *     ra    : hours ; dec, dis : degrees, AU ; rx,ry,rz : unit vector
 */

#include <stdio.h>
#include <stdlib.h>

#include "eph_manager.h"
#include "novas.h"

struct body_id {
  short number;
  const char* name;
};

struct site {
  short where;        /* 0 geocenter, 1 surface */
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

  cat_entry dummy_star;
  make_cat_entry("DUMMY", "xxx", 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, &dummy_star);

  const struct body_id bodies[] = {
      {1, "Mercury"}, {2, "Venus"},   {4, "Mars"},   {5, "Jupiter"},
      {6, "Saturn"},  {7, "Uranus"},  {8, "Neptune"},{9, "Pluto"},
      {10, "Sun"},    {11, "Moon"}};
  const int n_bodies = (int)(sizeof(bodies) / sizeof(bodies[0]));

  const short coord_systems[] = {0, 1, 2, 3};
  const short accuracies[] = {0, 1};        /* full, reduced */

  const struct site sites[] = {
      {0, 0.0, 0.0, 0.0, 0.0},              /* geocenter */
      {1, 40.0, -105.0, 1650.0, 69.184},    /* mid-north site */
      {1, -31.0, 149.0, 1100.0, 69.184}};   /* mid-south site */
  const int n_sites = (int)(sizeof(sites) / sizeof(sites[0]));

  /* Interior grid; keep a margin so the light-time antedated epoch stays in
   * range. */
  const int n_samples = 12;
  const double lo = jd_begin + 1.0;
  const double hi = jd_end - 1.0;
  const double step = (hi - lo) / (double)(n_samples + 1);

  printf("coord_sys,accuracy,body,where,lat,lon,height,delta_t,jd_tt,"
         "ra,dec,dis,rx,ry,rz,rv\n");

  long emitted = 0, skipped = 0;
  for (int ci = 0; ci < (int)(sizeof(coord_systems) / sizeof(short)); ++ci) {
    for (int ai = 0; ai < 2; ++ai) {
      for (int si = 0; si < n_sites; ++si) {
        observer obs;
        if (sites[si].where == 0)
          make_observer_at_geocenter(&obs);
        else
          make_observer_on_surface(sites[si].lat, sites[si].lon,
                                   sites[si].height, 10.0, 1010.0, &obs);
        for (int bi = 0; bi < n_bodies; ++bi) {
          object o;
          if (make_object(0, bodies[bi].number, (char*)bodies[bi].name,
                          &dummy_star, &o) != 0)
            continue;
          for (int ti = 1; ti <= n_samples; ++ti) {
            double jd_tt = lo + step * (double)ti;
            sky_pos sp;
            short e = place(jd_tt, &o, &obs, sites[si].delta_t,
                            coord_systems[ci], accuracies[ai], &sp);
            if (e != 0) {
              ++skipped;
              continue;
            }
            printf("%d,%d,%d,%d,%.17g,%.17g,%.17g,%.17g,%.17g,"
                   "%.17e,%.17e,%.17e,%.17e,%.17e,%.17e,%.17e\n",
                   coord_systems[ci], accuracies[ai], bodies[bi].number,
                   sites[si].where, sites[si].lat, sites[si].lon,
                   sites[si].height, sites[si].delta_t, jd_tt, sp.ra, sp.dec,
                   sp.dis, sp.r_hat[0], sp.r_hat[1], sp.r_hat[2], sp.rv);
            ++emitted;
          }
        }
      }
    }
  }

  fprintf(stderr, "emitted %ld place rows, %ld skipped\n", emitted, skipped);
  ephem_close();
  return 0;
}
