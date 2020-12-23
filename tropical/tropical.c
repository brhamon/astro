/*
 *  tropical.c: Display upcoming tropical moments
 *
 *  A tropical moment is one of the four points in the tropical year known
 *  as a "solstice" or an "equinox." The solstice corresponds to the local
 *  maximum or minimum of the latitude of the subsolar point, and the equinox
 *  corresponds to the crossing of the equator by the subsolar point.
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
#include <stdlib.h>
#include <sys/stat.h>
#include "bull_a.h"
#include <eph_manager.h>

static object sol;
static short int accuracy;
static const char jpleph_name[] = "JPLEPH";

/*
 * Calculate the geodesic coordinates of the subsolar point. (Or the
 * "sub-object point", when <obj> is not Sol.)
 * This is the location on Earth at which the object is at local zenith.
 */
short int transit_coord(time_parameters_t* tp, object* obj,
        double x_pole, double y_pole,
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

short int sample_transit_coord(moment_t* sample_ptr, double jd) {
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
    if ((error = transit_coord(&timep, &sol, x_pole, y_pole, &lat, &lon))
            != 0) {
        printf("Error %d in transit_coord.", error);
        return error;
    }
    sample_ptr->jd = jd;
    sample_ptr->lat = lat;
    sample_ptr->lon = lon;
    return error;
}

double subsolar_latitude(double jd) {
    moment_t mom;
    short int error = sample_transit_coord(&mom, jd);
    if (error != 0) {
        printf("%s: Error %d returned from sample_transit_coord.\n", __func__,
                error);
        exit(1);
    }
    return mom.lat;
}

int find_moments(double cur_jd, moment_t* outs, int outs_sz)
{
    int nbr_of_roots;
    int i;
    real_range hint;
    real_range ranges[12];
    double midp;
    double sol_jd;
    short int error;
    int nbr_of_outs=0;

    hint.lb = cur_jd - (sidereal_year_in_days * 0.51);
    hint.ub = cur_jd + (sidereal_year_in_days * 1.02);

    nbr_of_roots = bracket_roots(subsolar_latitude, &hint, NELTS(ranges), ranges,
            NELTS(ranges));
    if (nbr_of_roots < 3 || nbr_of_roots > 4) {
        printf("%s: ERROR incorrect number of roots found (%d).\n", __func__,
                nbr_of_roots);
        exit(1);
    }
    for (i=0; i < nbr_of_roots; ++i) {
        hint.lb = hint.ub;
        hint.ub = zbrent(subsolar_latitude, &ranges[i], one_second_jd);
        if (i != 0) {
            midp=0.5*(hint.ub + hint.lb);
            (void)brent(hint.lb, midp, hint.ub, subsolar_latitude, one_second_jd / midp,
                    &sol_jd, subsolar_latitude(midp) < 0.0);
            if (sol_jd >= cur_jd) {
                if (++nbr_of_outs < outs_sz) {
                    error = sample_transit_coord(outs, sol_jd);
                    if (error != 0) {
                        printf("%s(%d): ERROR %d returned from "
                                "sample_transit_coord.\n", __func__, __LINE__,
                                error);
                        exit(1);
                    }
                    ++outs;
                }
            }
        }
        if (hint.ub >= cur_jd) {
            if (++nbr_of_outs < outs_sz) {
                error = sample_transit_coord(outs, hint.ub);
                if (error != 0) {
                    printf("%s(%d): ERROR %d returned from "
                            "sample_transit_coord.\n", __func__, __LINE__,
                            error);
                    exit(1);
                }
                ++outs;
            }
        }
    }
    return nbr_of_outs;
}

static const char* tropical_moment_names[] = {
    "March", "June", "September", "December"
};

static const char* tropical_moment_types[] = {
    "equinox", "solstice", "equinox", "solstice"
};

void display_moment(moment_t* moment)
{
    double cur_j;
    short int year, month, day;
    short int hour, minute, second;
    int mom;
    char d1[DMS_MAX];
    char d2[DMS_MAX];

    cal_date(moment->jd, &year, &month, &day, &cur_j);
    hour = (short int)floor(cur_j);
    cur_j -= (double)hour;
    cur_j *= 60.0;
    minute = (short int)floor(cur_j);
    cur_j -= (double)minute;
    cur_j *= 60.0;
    second = (short int)floor(cur_j);
    mom = (month-1)/3;
    if (mom < 0 || mom > 3) {
        printf("%s: LOGIC ERROR moment (%d) out of range.\n", __func__, mom);
        exit(1);
    }

    printf("%s %s occurs at %d-%02d-%02d %02d:%02d:%02d UTC\n",
            tropical_moment_names[mom], tropical_moment_types[mom],
            year, month, day, hour, minute, second);
    printf("  Subsolar point: {%s %s} [{%.10lf, %.10lf}]\n", as_dms(d1, moment->lat, 1),
            as_dms(d2, moment->lon, 0), moment->lat, moment->lon);
}

void list_next_moments(int years)
{
    int pass;
    int i;
    int nbr_m;
    double cur_jd = get_jd_utc() - 5;
    moment_t m[6];
    for (pass=0; pass < years; ++pass) {
        nbr_m = find_moments(cur_jd, &m[0], NELTS(m));
        if (nbr_m == 0) {
            printf("%s: ERROR found no moments.\n", __func__);
            exit(1);
        }
        for (i=0; i<nbr_m; ++i) {
            display_moment(&m[i]);
        }
        cur_jd = m[nbr_m-1].jd + 1.0;
    }
}

static void get_ephfilename(char *workpath, size_t workpath_len, int f_check_file) {
    make_local_path();
    int sz = snprintf(workpath, workpath_len, "%s/%s", g_local_path, jpleph_name);
    if (sz < 0 || (size_t)sz >= workpath_len) {
        printf("error: unable to form %s/%s pathname.\n", g_local_path, jpleph_name);
        exit(1);
    }
    if (f_check_file != 0) {
        struct stat buffer;
        int status = stat(workpath, &buffer);
        if (status != 0) {
            printf("error: unable to locate %s.\n", jpleph_name);
            exit(1);
        }
    }
}

int main(void) {
    cat_entry dummy_star;
    short int error = 0;
    char ttl[85];

    char workpath[PATH_MAX];
    get_ephfilename(workpath, sizeof(workpath), 1);
    double jd_begin, jd_end;
    short int de_number;
    ephem_open(workpath, &jd_begin, &jd_end, &de_number);

    accuracy = 0;
    if ((error = bull_a_init()) != 0) {
        printf("Error %d from bull_a_init.", error);
        return error;
    }
    get_eph_title(ttl, sizeof(ttl), workpath);

    printf("Ephemeris: %s\n", ttl);

    make_cat_entry("DUMMY","xxx", 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, &dummy_star);

    if ((error = make_object(0, the_planets[9].id, (char*)the_planets[9].name,
		    &dummy_star, &sol)) != 0) {
        printf ("Error %d from make_object (%s)\n", error, the_planets[9].name);
        return error;
    }

    list_next_moments(2);
    bull_a_cleanup();
    ephem_close();
    return 0;
}
/* vim:set ts=4 sts=4 sw=4 cindent expandtab: */
