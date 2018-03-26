#include "jplint.h"
#include "jpleph.h"

void jplint_ (double *tjdtdb, long int *target, long int *center,
            double *posvel, long int *error)
{
    const int cdim = 600;
    char name[cdim][6];
    int32_t nvals, i;
    double limits[3], value[cdim];

    /*
     * Call JPL subroutine 'const' to get the time limits of the
     * ephemeris.
     */
    const_(name, value, limits, &nvals);

    /*
     * Check that the input time is within the ephemeris limits.
     * Return the position and velocity of the body if it is.
     */
    if ((*tjdtdb < limits[0]) || (*tjdtdb > limits[1])) {
        *error = 1;
        for (i=0; i<6; i++) {
            posvel[i] = 0.0;
        }
    } else {
        *error = 0;
        pleph(*tjdtdb, *target, *center, posvel);
    }
}

void jplihp_ (double *tjdtdb, long int *target, long int *center,
            double *posvel, long int *error)
{
    const int cdim = 600;
    char name[cdim][6];
    int32_t nvals, i;
    double limits[3], value[cdim];

    /*
     * Call JPL subroutine 'const' to get the time limits of the
     * ephemeris.
     */
    const_(name, value, limits, &nvals);

    double tjd = tjdtdb[0] + tjdtdb[1];
    /*
     * Check that the input time is within the ephemeris limits.
     * Return the position and velocity of the body if it is.
     */
    if ((tjd < limits[0]) || (tjd > limits[1])) {
        *error = 1;
        for (i=0; i<6; i++) {
            posvel[i] = 0.0;
        }
    } else {
        *error = 0;
        dpleph(tjdtdb, *target, *center, posvel);
    }
}


