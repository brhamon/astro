#include "moon.h"

const char *moon_phase_names[NBR_OF_MOON_PHASES] = {
    "full",
    "waxing gibbous",
    "first quarter",
    "waxing crescent",
    "new",
    "waning crescent",
    "last quarter",
    "waning gibbous"
};

/*
 * Calculate the equatorial spherical coordinates of the solar transit point with
 * respect to the center of the Earth-facing surface of the Moon.
 */
short int moon_phase(time_parameters_t* tp,
        object* sun, object* moon, short int accuracy,
        double* phlat, double* phlon, int* phindex)
{
    short int error = 0;
    observer geo_ctr;
    sky_pos t_place;
    double lon1, lat1;
    double lon2, lat2;
    double earth_sun[3];
    double moon_earth[3];
    double moon_sun[3];

    make_observer(0, NULL, NULL, &geo_ctr);
    if ((error = place(tp->jd_tt, sun, &geo_ctr, tp->delta_t,
                    coord_equ, accuracy, &t_place)) != 0) {
        printf("Error %d from place.", error);
        return error;
    }
    radec2vector(t_place.ra, t_place.dec, t_place.dis, earth_sun);
    if ((error = place(tp->jd_tt, moon, &geo_ctr, tp->delta_t,
                    coord_equ, accuracy, &t_place)) != 0) {
        printf("Error %d from place.", error);
        return error;
    }
    radec2vector(t_place.ra, t_place.dec, t_place.dis, moon_earth);
    /* The vector points from Earth to Moon. Reverse it. */
    moon_earth[0] *= -1.0;
    moon_earth[1] *= -1.0;
    moon_earth[2] *= -1.0;

    /* Calculate the position vector of the Sun w/r/t Moon */
    moon_sun[0] = earth_sun[0] + moon_earth[0];
    moon_sun[1] = earth_sun[1] + moon_earth[1];
    moon_sun[2] = earth_sun[2] + moon_earth[2];

    vector2radec(moon_earth, &lon1, &lat1);
    lon1 *= 15.0;
    if (lon1 > 180.0) {
        lon1 -= 360.0;
    }
    /* {lat1, lon1} is now the equatorial spherical coordinates of the
     * Earth transit point on the Moon. */

    vector2radec(moon_sun, &lon2, &lat2);
    lon2 *= 15.0;
    if (lon2 > 180.0) {
        lon2 -= 360.0;
    }
    /* {lat2, lon2} is now the equatorial spherical coordinates of the
     * Solar transit point on the Moon. */

    *phlon = normalize(lon2 - lon1, 360.0);
    *phindex = (int)floor(normalize(lon2 - lon1 + 22.5, 360.0)/45.0);
    if (*phlon > 180.0) {
        *phlon -= 360.0;
    }
    *phlat = lat2 - lat1;
    return error;
}


