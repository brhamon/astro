#include <novas.h>
#include <ephutil.h>
#include "bull_a.h"

typedef struct {
    double jd;
    double decr;
    double rar;
} moment_t;

/*
 * Calculate the 'Transit coordinates' for a solar system object.
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

/*
 * Kerry Shetline's excellent SkyViewCafe presents ZD as "Altitude",
 * and Azimuth is presented as "degrees West of South".
 *
 * This function adjusts the equ2hor ZD/AZ values to match SkyViewCafe.
 */
void correct_zd_az(double* zd, double* az) {
    *az = normalize(*az - 180.0, 360.0);
    *zd = 90.0 - *zd;
}

int main(void) {
/*
 * The following three values are Earth Orientation (EO) paramters published by IERS.
 * See bull_a.c for more information.
 */
    double x_pole = 0.0;
    double y_pole = 0.0;
    double ut1_utc = 0.0;

    const short int accuracy = 0;
    short int error = 0;

    /* at epoch J2000.0 */
    const double sidereal_year_in_days = 365.256363004;
    const double one_second_jd = 1.0 / 86400.0;

    time_parameters_t timep;
    double rar, decr, cur_j;
    int alpha, beta;
    int index;
    moment_t sample[5];
    int equ_type = 0;
    cat_entry dummy_star;
    object sol;
    bull_a_entry_t* ba_ent;

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

    cur_j = get_jd_utc();
    cur_j -= sidereal_year_in_days * 0.4;
    const unsigned mask_narrow = 0xE;
    const unsigned mask_wide = 0x15;
    unsigned cur_bit = 1;
    int accum_narrow = 0;
    int accum_wide = 0;
    for (index = 0; index < 5; index++) {
        ba_ent = bull_a_find(cur_j - 2400000.5);
        if (ba_ent == NULL) {
            x_pole = 0.0;
            y_pole = 0.0;
            ut1_utc = 0.0;
        } else {
            x_pole = ba_ent->pm_x;
            y_pole = ba_ent->pm_y;
            ut1_utc = ba_ent->ut1_utc;
        }
        make_time_parameters(&timep, cur_j, ut1_utc);
        /* Calculate Solar transit info. */
        if ((error = transit_coord(&timep, &sol, x_pole, y_pole, accuracy, &decr, &rar))
                != 0) {
            printf("Error %d in transit_coord.", error);
            return error;
        }
        sample[index].jd = cur_j;
        sample[index].decr = decr;
        sample[index].rar = rar;
        if (decr == 0.0) {
            printf("Equinox found\n");
            break;
        }
        if ((cur_bit & mask_narrow) != 0) {
            accum_narrow += (decr < 0.0) ? -1 : 1;
        }
        if ((cur_bit & mask_wide) != 0) {
            accum_wide += (decr < 0.0) ? -1 : 1;
        }
        cur_j += sidereal_year_in_days * 0.2;
        cur_bit <<= 1;
    }
    /* [-1, -1, -1] -3 Use wide
     * [-1, -1,  1] -1 March equinox forward
     * [-1,  1, -1] -1 Impossible! Narrow samples too close together for this to happen.
     * [-1,  1,  1]  1 March equinox backward
     * [ 1, -1, -1] -1 September equinox backward
     * [ 1, -1,  1]  1 Impossible!
     * [ 1,  1, -1]  1 September equinox backward
     * [ 1,  1,  1]  3 Use wide
     */
    switch (accum_narrow) {
        case -1:
            if (sample[3].decr > 0.0) {
                equ_type = 1; /* March */
                index = 3;
            } else {
                equ_type = -1; /* September */
                index = 1;
            }
            break;
        case 1:
            if (sample[1].decr < 0.0) {
                equ_type = 1; /* March */
                index = 1;
            } else {
                equ_type = -1; /* September */
                index = 3;
            }
            break;
        case 3:
            /* Use wide */
            switch (accum_wide) {
                case -1:
                    if (sample[4].decr > 0.0) {
                        equ_type = 1; /* March */
                        index = 4;
                    } else {
                        equ_type = -1; /* September */
                        index = 0;
                    }
                    break;
                case 1:
                    if (sample[0].decr < 0.0) {
                        equ_type = 1; /* March */
                        index = 0;
                    } else {
                        equ_type = -1; /* September */
                        index = 4;
                    }
                    break;
                default:
                    printf("Logic error at line %d\n", __LINE__);
                    return 1;
            }
        default:
            printf("Logic error!\n");
    }
    alpha = index < 2 ? index : 2;
    beta = index >= 2 ? index : 2;
    if (beta - alpha == 1) {
        if (alpha == 0) {
            printf("Logic error at line %d\n", __LINE__);
            return 1;
        }
        sample[alpha - 1] = sample[alpha];
        alpha--;
    }
    if (beta - alpha != 2) {
        printf("Logic error at line %d\n", __LINE__);
        return 1;
    }
    index = beta - 1; /* where the new mid-point sample will go */
    for (;;) {
        cur_j = (sample[beta].jd + sample[alpha].jd) / 2.0;
        if ((sample[beta].jd - sample[alpha].jd) < one_second_jd) {
            break;
        }
        ba_ent = bull_a_find(cur_j - 2400000.5);
        if (ba_ent == NULL) {
            x_pole = 0.0;
            y_pole = 0.0;
            ut1_utc = 0.0;
        } else {
            x_pole = ba_ent->pm_x;
            y_pole = ba_ent->pm_y;
            ut1_utc = ba_ent->ut1_utc;
        }
        make_time_parameters(&timep, cur_j, ut1_utc);
        /* Calculate Solar transit info. */
        if ((error = transit_coord(&timep, &sol, x_pole, y_pole, accuracy, &decr, &rar))
                != 0) {
            printf("Error %d in transit_coord.", error);
            return error;
        }
        sample[index].jd = cur_j;
        sample[index].decr = decr;
        sample[index].rar = rar;

        /* March:   alpha sample +, beta sample -
         *    decr + => go right
         * September: alpha sample -, beta sample +
         *    decr + => go left
         */
        if ((equ_type < 0 && decr < 0.0) || (equ_type > 0 && decr > 0.0)) {
            /* GO LEFT */
            sample[beta] = sample[index];
        } else {
            /* GO RIGHT */
            sample[alpha] = sample[index];
        }
    }

    short int year, month, day;
    short int hour, minute, second;

    cal_date (sample[alpha].jd, &year, &month, &day, &cur_j);
    hour = (short int)floor(cur_j);
    cur_j -= (double)hour;
    cur_j *= 60.0;
    minute = (short int)floor(cur_j);
    cur_j -= (double)minute;
    cur_j *= 60.0;
    second = (short int)floor(cur_j);

    printf("%s equinox occurs at %d-%02d-%02d %02d:%02d:%02d UTC\n",
            equ_type > 0 ? "March" : "September",
            year, month, day, hour, minute, second);

    bull_a_cleanup();

    (void)sidereal_year_in_days;
    return 0;
}
/* vim:set ts=4 sts=4 sw=4 cindent expandtab: */
