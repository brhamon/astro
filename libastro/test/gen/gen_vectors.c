/*
 * gen_vectors.c -- reference test-vector generator (the oracle).
 *
 * Links the legacy NOVAS-C archive (../../novasc3.1/novas.a) and dumps
 * planet_ephemeris() outputs to stdout as CSV. The C++ Layer-0 tests replay
 * these and must match to round-off (research doc 3.5).
 *
 * The grid is chosen to exercise every state() code path:
 *   - target/center reconstruction branches (SSB, EMB, Earth, Moon, direct
 *     Earth<->Moon both ways, body-to-body with Earth/Moon as center);
 *   - both unit systems (AU/day and km/s -- the eph_manager KM flag);
 *   - two-part Julian dates (jd_lo != 0), incl. the recommended midnight split;
 *   - boundary epochs (span ends, record boundary, sub-interval boundaries).
 *
 * Usage:
 *   gen_vectors <path-to-ephemeris> [out.csv]   (stdout if out.csv omitted)
 *
 * CSV columns:
 *   units,target,center,jd_hi,jd_lo,px,py,pz,vx,vy,vz
 *     units : 0 = AU, AU/day ; 1 = km, km/s
 *     jd    : TDB, split as jd_hi + jd_lo (passed verbatim to planet_ephemeris)
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "eph_manager.h"

/* KM is an eph_manager global (0 = AU/day default, 1 = km/s). */
extern short int KM;

struct pair {
  short target;
  short center;
};

/* Point/state numbering: 0..8 Mercury..Pluto, 9 Moon, 10 Sun, 11 SSB, 12 EMB. */
static const struct pair pairs[] = {
    /* heliocentric planets (regular reads, Sun as center) */
    {0, 10}, {1, 10}, {3, 10}, {4, 10}, {5, 10}, {6, 10}, {7, 10}, {8, 10},
    /* barycentric (SSB as center -> zero-center path) */
    {0, 11}, {2, 11}, {9, 11}, {10, 11}, {12, 11}, {4, 11},
    /* Earth/Moon system reconstruction */
    {2, 9},  {9, 2},  {2, 12}, {9, 12}, {2, 10}, {9, 10}, {12, 10},
    /* Earth/Moon as center for other bodies */
    {3, 2},  {4, 2},  {10, 9},
    /* SSB as target (zero-target path) */
    {11, 2},
};

struct epoch {
  double hi;
  double lo;
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

  /* Record span in days (SS[2]); needed for boundary epochs. */
  const double step = SS[2];
  fprintf(stderr, "opened DE%d, JD %.1f .. %.1f, %.0f d/rec\n", de_number,
          jd_begin, jd_end, step);

  struct epoch epochs[512];
  int n_epochs = 0;

  /* Dense grid across the span, alternating single-part and midnight-split
   * forms so both the jd_lo == 0 and jd_lo != 0 paths are covered. */
  const int n_grid = 100;
  const double grid_step = (jd_end - jd_begin) / (double)(n_grid + 1);
  for (int i = 1; i <= n_grid; ++i) {
    double total = jd_begin + grid_step * (double)i;
    if (i & 1) {
      epochs[n_epochs].hi = total;
      epochs[n_epochs].lo = 0.0;
    } else {
      double hi = floor(total - 0.5) + 0.5; /* most recent midnight */
      epochs[n_epochs].hi = hi;
      epochs[n_epochs].lo = total - hi;
    }
    ++n_epochs;
  }

  /* Boundary / edge epochs.
   *
   * NOTE: the *exact* jd_end is intentionally excluded. There eph_manager's
   * `nr -= 2` branch sets the normalized time to 2.0, so the Chebyshev series
   * is evaluated at tc = 3.0 -- extrapolation that blows up to ~1e16 and is not
   * a meaningful equivalence target (both implementations diverge in the noise
   * of that amplification). The modern library may later reject exact-jd_end
   * outright; for now state() stays faithful and we test up to jd_end - eps. */
  double specials[] = {
      jd_begin,             /* first data record, t0 == 0 */
      jd_begin + 1.0e-6,    /* just inside the start */
      jd_end - 1.0e-6,      /* just inside the end */
      jd_begin + step,      /* start of the 2nd data record */
      jd_begin + step - 1.0e-9, /* end of the 1st data record */
      jd_begin + 8.0,       /* Mercury sub-interval boundary (na=4, 8 d each) */
      jd_begin + 16.0, jd_begin + 24.0,
      (jd_begin + jd_end) * 0.5, /* mid-span */
  };
  for (int i = 0; i < (int)(sizeof(specials) / sizeof(specials[0])); ++i) {
    epochs[n_epochs].hi = specials[i];
    epochs[n_epochs].lo = 0.0;
    ++n_epochs;
  }

  const int n_pairs = (int)(sizeof(pairs) / sizeof(pairs[0]));
  printf("units,target,center,jd_hi,jd_lo,px,py,pz,vx,vy,vz\n");

  long emitted = 0, skipped = 0;
  for (int u = 0; u <= 1; ++u) {
    KM = (short)u; /* 0 -> AU/day, 1 -> km/s */
    for (int e = 0; e < n_epochs; ++e) {
      double tjd[2] = {epochs[e].hi, epochs[e].lo};
      for (int p = 0; p < n_pairs; ++p) {
        double pos[3], vel[3];
        short st = planet_ephemeris(tjd, pairs[p].target, pairs[p].center, pos,
                                    vel);
        if (st != 0) {
          ++skipped; /* e.g. epoch just out of range after the split */
          continue;
        }
        printf("%d,%d,%d,%.17g,%.17g,"
               "%.17e,%.17e,%.17e,%.17e,%.17e,%.17e\n",
               u, pairs[p].target, pairs[p].center, epochs[e].hi, epochs[e].lo,
               pos[0], pos[1], pos[2], vel[0], vel[1], vel[2]);
        ++emitted;
      }
    }
  }

  fprintf(stderr, "emitted %ld rows (%d epochs x %d pairs x 2 units), %ld "
                  "skipped\n",
          emitted, n_epochs, n_pairs, skipped);

  ephem_close();
  return 0;
}
