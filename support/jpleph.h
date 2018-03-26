#ifndef _JPLEPH_H_
#define _JPLEPH_H_

#include <inttypes.h>

extern void const_(char nam[][6], double val[], double sss[], int32_t *n);
extern void state(double et2[],int32_t list[],double pv[][6],double nut[]);
extern void split(double tt, double fr[]);
extern void interp(double buf[],double t[],int32_t ncf,int32_t ncm,int32_t na,int32_t ifl,
            double pv[]);
extern void dpleph(double et2[2],int32_t ntarg,int32_t ncent,double rrd[] );

static inline void pleph(double et,int32_t ntarg,int32_t ncent,double rrd[] ) {
    double et2[2] = { et, 0.0 };
    dpleph(et2, ntarg, ncent, rrd);
}

/*
 * IMPORTANT:
 * Set g_ephfile_name to the ephemeris file before calling any NOVAS-C function.
 */
extern const char *g_ephfile_name;

/*
 * Call this when your program is finished using the JPL ephemeris.
 */
void finalize_jpleph();
#endif
