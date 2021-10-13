#ifndef MOON_H_
#define MOON_H_ 1

#include <novas.h>
#include "ephutil.h"

#define NBR_OF_MOON_PHASES 8

extern const char *moon_phase_names[NBR_OF_MOON_PHASES];

/*
 * Calculate the equatorial spherical coordinates of the solar transit point with
 * respect to the center of the Earth-facing surface of the Moon.
 * Because of tidal locking, the Moon always points the same face to Earth.
 * The center of this face is the phase origin. When the phase is {0,0}, it
 * means that the Earth and Sun transit points on the Moon are coincident.
 * Positive phase latitudes mean the Sun transit point is north of the Earth
 * transit point. Positive phase longitudes mean the Sun transit point is east
 * of the Sun transit point.
 *
 * Input arguments:
 *   tp - pointer to the time_parameters_t structure containing the Julian Date
 *        and TT for the moment of interest.
 *   sun - pointer to the object structure initialized for the Sun
 *   moon - pointer to the object structure initialized for the Moon
 *   accuracy - Selection for accuracy (0=full, 1=reduced)
 * Output arguments:
 *   phlat - pointer to a double that will be assigned the phase latitude.
 *           This is the number of degrees north of the Earth transit point
 *           on the Moon.
 *   phlon - pointer to a double that will be assigned the phase longitude.
 *           This is the number of degrees east of the Earth transit point
 *           on the Moon.
 *   phindex - pointer to an int that will be assigned a number from 0 to 7,
 *           corresponding to one of the common names of the Moon phases.
 *           Use moon_phase_names[phindex] to obtain a string name for the
 *           Moon phase.
 */
short int moon_phase(time_parameters_t* tp,
        object* sun, object* moon, short int accuracy,
        double* phlat, double* phlon, int* phindex);

#endif
