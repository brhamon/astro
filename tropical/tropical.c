/*
 *  tropical.c: Display the next two tropical moments
 *
 *  A tropical moment is one of the four points in the
 *  tropical year known as a "solstice" or an "equinox."
 *  The solstice corresponds to the local maximum or
 *  minimum of the latitude of the subsolar point,
 *  and the equinox corresponds to the crossing of the
 *  equator by the subsolar point.
 *
 *  Derived from software developed by:
 *
 *  U. S. Naval Observatory
 *  Astronomical Applications Dept.
 *  Washington, DC
 *  http://aa.usno.navy.mil/software/novas/novas_info.php
 */
#include <novas.h>
#include <ephutil.h>
#include <string.h>
#include "bull_a.h"

/* at epoch J2000.0 */
static const double sidereal_year_in_days = 365.256363004;
static const double one_second_jd = 1.0 / 86400.0;

typedef struct {
    double jd;
    double lat;
    double lon;
} moment_t;

/*
 * Calculate the geodesic coordinates of the subsolar point. (Or the
 * "sub-object point", when <obj> is not Sol.)
 * This is the location on Earth at which the object is at local zenith.
 */
short int transit_coord(time_parameters_t* tp, object* obj,
        double x_pole, double y_pole,
        short int accuracy,
        double* lat, double* lon)
{
    short int error = 0;
    observer geo_ctr;
    sky_pos t_place;
    double pose[3];
    double pos[3];

    make_observer(0, NULL, NULL, &geo_ctr);
    if ((error = place(tp->jd_tt, obj, &geo_ctr, tp->delta_t,
                    coord_equ, accuracy, &t_place)) != 0) {
        printf("Error %d from place.", error);
    } else {
        radec2vector(t_place.ra, t_place.dec, t_place.dis, pose);
        if ((error = cel2ter(tp->jd_ut1, 0.0, tp->delta_t, coord_equ, accuracy,
                        coord_equ, x_pole, y_pole, pose, pos)) != 0) {
            printf("Error %d from cel2ter.", error);
        } else {
            vector2radec(pos, lon, lat);
            *lon *= 15.0;
            if (*lon > 180.0) {
                *lon -= 360.0;
            }
        }
    }
    return error;
}

short int calculate_sample(moment_t* sample_ptr, double jd, object* sol_ptr,
        short int accuracy) {
    double x_pole, y_pole, ut1_utc; // Bulletin A corrective factors
    double lon, lat;                // transit coordinates
    time_parameters_t timep;

    short int error = 0;

    bull_a_entry_t* ba_ent = bull_a_find(jd - 2400000.5);
    if (ba_ent == NULL) {
        x_pole = 0.0;
        y_pole = 0.0;
        ut1_utc = 0.0;
    } else {
        x_pole = ba_ent->pm_x;
        y_pole = ba_ent->pm_y;
        ut1_utc = ba_ent->ut1_utc;
    }
    make_time_parameters(&timep, jd, ut1_utc);
    /* Calculate Solar transit info. */
    if ((error = transit_coord(&timep, sol_ptr, x_pole, y_pole, accuracy, &lat, &lon))
            != 0) {
        printf("Error %d in transit_coord.", error);
        return error;
    }
    sample_ptr->jd = jd;
    sample_ptr->lat = lat;
    sample_ptr->lon = lon;
    return error;
}

/*
 * Define the index of the samples for ALPHA (lower bound),
 * BETA (upper bound), and GAMMA (mid-point).
 */
#define ALPHA          0
#define GAMMA          1
#define BETA           2
#define NBR_OF_SAMPLES 3

#define FALSE 0
#define TRUE (!FALSE)

short int calculate_next_equinox(moment_t* equinox, double cur_j, object* sol_ptr,
        short int accuracy) {
    short int error = 0;
    int index;
    moment_t sample[NBR_OF_SAMPLES];
    int equ_type = 0;
    cur_j -= sidereal_year_in_days * 0.2;
    int sum = 0;
    int f_once = FALSE;
    int f_goleft = FALSE;
    /*
     * Coarse find. Take samples of the transit coordinates in one-fifth year
     * resolution.
     */
    index = 0;
    for (;;) {
        for (; index < NBR_OF_SAMPLES; index++) {
            if ((error = calculate_sample(&sample[index], cur_j, sol_ptr, accuracy))
                    != 0) {
                printf("Error %d in calculate_sample.", error);
                return error;
            }
            sum += (sample[index].lat < 0.0) ? -1 : 1;
            cur_j += sidereal_year_in_days * 0.2;
        }
        /*
         *    sample
         *   0   1   2  sum   interpretation
         * [-1, -1, -1]  -3   Too close to December solstice. Advance & retry
         * [-1, -1,  1]  -1   March equinox is after sample[GAMMA].jd
         * [-1,  1, -1]  -1   Impossible! (sampling range is only 0.4 year)
         * [-1,  1,  1]   1   March equinox is before sample[GAMMA].jd
         * [ 1, -1, -1]  -1   September equinox is before sample[GAMMA].jd
         * [ 1, -1,  1]   1   Impossible! (sampling range is only 0.4 year)
         * [ 1,  1, -1]   1   September equinox is after sample[GAMMA].jd
         * [ 1,  1,  1]   3   Too close to June solstice. Advance & retry
         */
        if (sum == 1) {
            if (sample[ALPHA].lat < 0.0) {
                /* March equinox is before sample[GAMMA].jd */
                /*
                 * Only go 'left' if this is not the first loop iteration.
                 * This restriction keeps this function from seeking a moment
                 * prior to the input Julian Date.
                 */
                if (f_once) {
                    f_goleft = TRUE;
                    break;
                }
            } else {
                /* September equinox is after sample[GAMMA].jd */
                equ_type = -1;
                break;
            }
        } else if (sum == -1) {
            if (sample[BETA].lat >= 0.0) {
                /* March equinox is after sample[GAMMA].jd */
                equ_type = 1;
                break;
            } else {
                /* September equinox is before sample[GAMMA].jd */
                if (f_once) {
                    f_goleft = TRUE;
                    break;
                }
            }
        } else if (sum != 3 && sum != -3) {
            printf("Logic error! sum=%d\n", sum);
            error = 1;
            return error;
        }
        --index;
        sum -= (sample[ALPHA].lat < 0.0) ? -1 : 1;
        memmove(&sample[ALPHA], &sample[GAMMA], 2*sizeof(moment_t));
        f_once = TRUE;
    }
    if (f_goleft) {
        /* go left */
        sample[BETA] = sample[GAMMA];
    } else {
        /* go right */
        sample[ALPHA] = sample[GAMMA];
    }
    for (;;) {
        if ((sample[BETA].jd - sample[ALPHA].jd) < one_second_jd) {
            break;
        }
        cur_j = (sample[BETA].jd + sample[ALPHA].jd) / 2.0;
        if ((error = calculate_sample(&sample[GAMMA], cur_j, sol_ptr, accuracy)) != 0) {
            printf("Error %d in calculate_sample.", error);
            return error;
        }
        /* March:   ALPHA sample positive, BETA sample negative
         *    lat positive => go right
         * September: ALPHA sample negative, BETA sample positive
         *    lat positive => go left
         */
        if ((equ_type < 0 && sample[GAMMA].lat < 0.0) ||
                (equ_type > 0 && sample[GAMMA].lat > 0.0)) {
            /* go left */
            sample[BETA] = sample[GAMMA];
        } else {
            /* go right */
            sample[ALPHA] = sample[GAMMA];
        }
    }
    *equinox = sample[ALPHA];
    return error;
}

short int calculate_next_solstice(moment_t* moment, double cur_j, object* sol_ptr,
        short int accuracy) {
    int index;
    moment_t sample[NBR_OF_SAMPLES];
    int sol_type = 0;
    short int error = 0;
    cur_j -= sidereal_year_in_days * 0.4;
    for (;;) {
        for (index = 0; index < NBR_OF_SAMPLES; index++) {
            if ((error = calculate_sample(&sample[index], cur_j, sol_ptr, accuracy))
                    != 0) {
                return error;
            }
            cur_j += sidereal_year_in_days * 0.4;
        }
        if (sample[GAMMA].lat > 0.0) {
            if (sample[ALPHA].lat < sample[BETA].lat) {
                sol_type = 1; /* June solstice */
                cur_j = sample[GAMMA].jd +
                    sidereal_year_in_days * (sample[ALPHA].lat < 0.0 ? 0.25 : 0.15);
                break;
            }
        } else {
            if (sample[ALPHA].lat > sample[BETA].lat) {
                sol_type = -1; /* December solstice */
                cur_j = sample[GAMMA].jd +
                    sidereal_year_in_days * (sample[ALPHA].lat > 0.0 ? 0.25 : 0.15);
                break;
            }
        }
        cur_j = sample[GAMMA].jd + sidereal_year_in_days * 0.2;
    }
    /* exit conditions: 
     * sample[GAMMA] is *before* the target solstice
     * cur_j is *after* the target solstice
     * There is exactly one solstice of type sol_type in between.
     */
    sample[ALPHA] = sample[GAMMA];
    if ((error = calculate_sample(&sample[BETA], cur_j, sol_ptr, accuracy)) != 0) {
        return error;
    }

    for (;;) {
        cur_j = (sample[BETA].jd + sample[ALPHA].jd) / 2.0;
        if ((sample[BETA].jd - sample[ALPHA].jd) < one_second_jd) {
            break;
        }
        if ((error = calculate_sample(&sample[GAMMA], cur_j, sol_ptr, accuracy)) != 0) {
            return error;
        }

        /* December:
         *    take GAMMA and MIN(ALPHA, BETA)
         * June:
         *    take GAMMA and MAX(ALPHA, BETA)
         */
        if ((sol_type < 0 && sample[ALPHA].lat < sample[BETA].lat) ||
                (sol_type > 0 && sample[ALPHA].lat > sample[BETA].lat)) {
            /* GO LEFT */
            sample[BETA] = sample[GAMMA];
        } else {
            /* GO RIGHT */
            sample[ALPHA] = sample[GAMMA];
        }
    }
    *moment = sample[ALPHA];
    return error;
}

static const char* tropical_moment_names[] = {
    "March", "June", "September", "December"
};

void display_moment(const char* moment_str, moment_t* moment) {
    double cur_j;
    short int year, month, day;
    short int hour, minute, second;

    cal_date(moment->jd, &year, &month, &day, &cur_j);
    hour = (short int)floor(cur_j);
    cur_j -= (double)hour;
    cur_j *= 60.0;
    minute = (short int)floor(cur_j);
    cur_j -= (double)minute;
    cur_j *= 60.0;
    second = (short int)floor(cur_j);

    printf("%s %s occurs at %d-%02d-%02d %02d:%02d:%02d UTC\n",
            tropical_moment_names[(month-1)/3],
            moment_str, year, month, day, hour, minute, second);
    printf("Subsolar point: {%15.10f %15.10f}\n", moment->lat, moment->lon);
}

int main(void) {
    double cur_j;
    moment_t solstice;
    moment_t equinox;
    cat_entry dummy_star;
    object sol;
    short int error = 0;
    short int accuracy = 0;

    char ttl[85];

    if ((error = bull_a_init()) != 0) {
        printf("Error %d from bull_a_init.", error);
        return error;
    }
    get_eph_title(ttl, sizeof(ttl), "JPLEPH");

    printf ("Using solsys version 2\n%s\n\n", ttl);

    make_cat_entry("DUMMY","xxx", 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, &dummy_star);

    if ((error = make_object(0, the_planets[9].id, (char*)the_planets[9].name,
		    &dummy_star, &sol)) != 0) {
        printf ("Error %d from make_object (%s)\n", error, the_planets[9].name);
        return error;
    }

    cur_j = get_jd_utc() - 1;
    if ((error = calculate_next_solstice(&solstice, cur_j, &sol, accuracy)) != 0) {
        printf("Error %d in calculate_next_solstice\n", error);
        return error;
    }
    if ((error = calculate_next_equinox(&equinox, cur_j, &sol, accuracy)) != 0) {
        printf("Error %d in calculate_next_equinox\n", error);
        return error;
    }
    if (solstice.jd < equinox.jd) {
        display_moment("solstice", &solstice);
        display_moment("equinox", &equinox);
    } else {
        display_moment("equinox", &equinox);
        display_moment("solstice", &solstice);
    }

    bull_a_cleanup();

    return 0;
}
/* vim:set ts=4 sts=4 sw=4 cindent expandtab: */
