/*
 *  planets.c: Display zd/az of planets from ephemeris data
 *
 *  Derived from software developed by:
 *
 *  U. S. Naval Observatory
 *  Astronomical Applications Dept.
 *  Washington, DC
 *  http://aa.usno.navy.mil/software/novas/novas_info.php
 */

#define _XOPEN_SOURCE

#include <novas.h>
#include <ephutil.h>
#include <moon.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "bull_a.h"
#include <time.h>
#include <jpleph.h>

static char our_prog_name[64];
static const char jpleph_name[] = "JPLEPH";

/*
 * Global settings.
 */
int f_azi_s = 0;
int f_bull_a = 1;

/*
 * Information for Polaris (HD8890) is taken from the SKY2000 Master Catalog, Version 5.
 * The catalog was downloaded from the SIMBAD Astronomical Database. (file size: 30.4M)
 *
 * Direct link:
 * http://cdsarc.u-strasbg.fr/viz-bin/nph-Cat/tar.gz?V%2F145
 *
 * Mirror in the United States:
 * http://vizier.cfa.harvard.edu/viz-bin/nph-Cat/tar.gz?V%2F145
 *
 */
static cat_entry polaris_cat = {"POLARIS", "SKI", 2480053, 2.5303010278, 89.2641094444,
               3442.95000, -11.8,  7.56, -17.4};

/*
 * Display Greenwich and local apparent sidereal time and Earth Rotation Angle.
 *
 * These values help explain the instantaneous transalation between ICRS and ITRS.
 */
short int display_rotation(time_parameters_t *tp, on_surface *lp, short int accuracy)
{
    double gast;
    short int error = sidereal_time(tp->jd_ut1, 0.0, tp->delta_t, 1, 1, accuracy, &gast);
    if (error != 0) {
        printf_if(3, "ERRCODE %d from sidereal_time.", error);
        goto out;
    }

    double last = gast + lp->longitude / 15.0;
    if (last >= 24.0) {
        last -= 24.0;
    } else if (last < 0.0) {
        last += 24.0;
    }

    double theta = era(tp->jd_ut1, 0.0);

    printf_if(1, "Sidereal time: Greenwich=%16.11f, local=%16.11f\n", gast, last);
    printf_if(1, "ERA %15.10f\n\n", theta);
out:
    return error;
}

/*
 * Calculate the 'Transit coordinates' for a solar system object.
 * This is the location on Earth at which the object is at local zenith.
 */
short int transit_coord(time_parameters_t *tp, object *obj,
        double x_pole, double y_pole,
        short int accuracy,
        double *lat, double *lon)
{
    observer geo_ctr;
    sky_pos t_place;
    double pose[3];
    double pos[3];

    make_observer_at_geocenter(&geo_ctr);
    short int error = place(tp->jd_tt, obj, &geo_ctr, tp->delta_t, coord_equ, accuracy, &t_place);
    if (error != 0) {
        printf_if(3, "ERRCODE %d from place.", error);
        goto out;
    }
    radec2vector(t_place.ra, t_place.dec, t_place.dis, pose);
    error = cel2ter(tp->jd_ut1, 0.0, tp->delta_t, coord_equ, accuracy,
                    coord_equ, x_pole, y_pole, pose, pos);
    if (error != 0) {
        printf_if(3, "ERRCODE %d from cel2ter.", error);
        goto out;
    }
    vector2radec(pos, lon, lat);
    *lon *= 15.0;
    if (*lon > 180.0) {
        *lon -= 360.0;
    }
out:
    return error;
}

/*
 * NASA JPL expresses azimuth as degrees East of North, which is
 * the default representation. However, Kerry Shetline's excellent
 * SkyViewCafe presents ZD as "Altitude", and Azimuth is presented
 * as "degrees West of South".
 *
 * When the global <f_azi_s> is non-zero, express azimuth as
 * degrees West of South instread of degrees East of North.
 *
 * This function adjusts the equ2hor ZD/AZ values to match SkyViewCafe.
 */
void correct_zd_az(double *zd, double *az) {
    if (f_azi_s) {
        *az = normalize(*az - 180.0, 360.0);
    } else {
        *az = normalize(*az, 360.0);
    }
    *zd = 90.0 - *zd;
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

int planets_main(const on_surface *obs, struct tm *utc) {
/*
 * The following three values are Earth Orientation (EO) paramters published by IERS.
 * See bull_a.c for more information.
 */
    double x_pole = 0.0;
    double y_pole = 0.0;
    double ut1_utc = 0.0;

    const short int accuracy = 0;

    time_parameters_t timep;
    double zd, az, rar, decr, tmp;
    on_surface geo_loc;
    observer obs_loc;
    cat_entry dummy_star;
    object obj[NBR_OF_PLANETS];
    object polaris;
    sky_pos t_place;
    int index, earth_index=-1;
    double phlat, phlon;
    int phindex;

    char dec_str[DMS_MAX];
    char zd_str[DMS_MAX];
    char ra_str[HMS_MAX];
    char az_str[DMS_MAX];
    char ttl[85];
    char dt_str[32];

    short int error = 0;
    if (f_bull_a) {
        error = bull_a_init();
        if (error != 0) {
            printf("Error %d from bull_a_init.", error);
            goto out2;
        }
    }
    make_on_surface(obs->latitude, obs->longitude, obs->height, obs->temperature,
            obs->pressure, &geo_loc);
    make_observer_on_surface(obs->latitude, obs->longitude, obs->height, obs->temperature,
            obs->pressure, &obs_loc);

    make_cat_entry("DUMMY","xxx", 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, &dummy_star);

    for (index=0; index < NBR_OF_PLANETS; ++index) {
        if (the_planets[index].id == 3) {
            earth_index = index;
        }
        error = make_object(0, the_planets[index].id, (char *)the_planets[index].name,
                        &dummy_star, &obj[index]);
        if (error != 0 ) {
            printf_if(3, "ERRCODE %d from make_object (%s)\n", error, the_planets[index].name);
            goto out;
        }
    }
    error = make_object(2, 0, "Polaris", &polaris_cat, &polaris);
    if (error != 0) {
        printf_if(3, "ERRCODE %d from make_object (%s)\n", error, polaris_cat.starname);
        goto out;
    }

    char workpath[PATH_MAX];
    get_ephfilename(workpath, sizeof(workpath), 0);
    get_eph_title(ttl, sizeof(ttl), workpath);
    printf_if(1, "Ephemeris: %s\n", ttl);

    printf_if(1, "Observer geodetic location (ITRS = WGS-84):\n"
            "{ %s %s %6.1f } or { %15.10f %15.10f }\n\n",
           as_dms(zd_str, geo_loc.latitude, 1), as_dms(az_str, geo_loc.longitude, 0),
           geo_loc.height, geo_loc.latitude, geo_loc.longitude);

    strftime(dt_str, sizeof(dt_str), "%X %x", utc);
    printf_if(0, "%*sObserved at %s UTC\n", 85 - 16 - strlen(dt_str), "", dt_str);
    tmp = time_to_jd(utc);
    if (f_bull_a) {
        bull_a_entry_t *ba_ent = bull_a_find(tmp - 2400000.5);
        if (ba_ent != NULL) {
            printf_if(1, "Bulletin A: %d/%02d/%d %8.2f q=%c (%9.6f, %9.6f) %10.7f\n\n",
                    ba_ent->month, ba_ent->day, ba_ent->year, ba_ent->mjd,
                    ba_ent->pm_quality, ba_ent->pm_x, ba_ent->pm_y, ba_ent->ut1_utc);
            x_pole = ba_ent->pm_x;
            y_pole = ba_ent->pm_y;
            ut1_utc = ba_ent->ut1_utc;
        }
    }
    make_time_parameters(&timep, tmp, ut1_utc);
    printf_if(1, "JD=%15.6f TT=%15.6f UT1=%15.6f delta-T=%16.11f leapsecs=%5.1f\n\n",
            tmp, timep.jd_tt, timep.jd_ut1, timep.delta_t, timep.leapsecs);
    display_rotation(&timep, &geo_loc, accuracy);

    printf("%8s %12s %13s  %18s  %13s  %13s\n", "Object", "RA", "DEC", "DIST", "ZA",
            "AZ");
    for (index=0; index < NBR_OF_PLANETS; ++index) {
        if (index == earth_index) {
            continue; // skip Earth
        }
        error = place(timep.jd_tt, &obj[index], &obs_loc, timep.delta_t, coord_equ,
                        accuracy, &t_place);
        if (error != 0) {
            printf_if(3, "ERRCODE %d from place.", error);
            goto out;
        }

        /*
         * Position of the planet in local horizon coordinates. Correct for refraction.
         */
        equ2hor(timep.jd_ut1, timep.delta_t, accuracy, x_pole, y_pole, &geo_loc,
                t_place.ra, t_place.dec, 2, &zd, &az, &rar, &decr);
        correct_zd_az(&zd, &az);
        printf("%8s %s %s  %18.12e  %s  %s\n",
               obj[index].name, as_hms(ra_str, t_place.ra),
               as_dms(dec_str, t_place.dec, 2), t_place.dis, as_dms(zd_str, zd, 2),
               as_dms(az_str, az, 2));
    }

    /* Polaris provides a helpful frame of reference for the other numbers, since
     * it's always very close to the CIP.
     */
    error = place(timep.jd_tt, &polaris, &obs_loc, timep.delta_t, coord_equ,
                    accuracy, &t_place);
    if (error != 0) {
        printf_if(3, "ERRCODE %d from place.", error);
        goto out;
    }
    equ2hor(timep.jd_ut1, timep.delta_t, accuracy, x_pole, y_pole, &geo_loc,
            t_place.ra, t_place.dec, 2, &zd, &az, &rar, &decr);
    correct_zd_az(&zd, &az);

    /* equ2hor leaves the distance at 0.0. Provide an estimate in AU. */
    double paralx = polaris_cat.parallax;
    if (paralx <= 0.0) {
        paralx = 1.0e-6;
    }
    t_place.dis = 1.0 / sin (paralx * 1.0e-3 * ASEC2RAD);

    printf("%8s %s %s  %18.12e  %s  %s\n",
            polaris_cat.starname, as_hms(ra_str, t_place.ra),
            as_dms(dec_str, t_place.dec, 2), t_place.dis, as_dms(zd_str, zd, 2),
            as_dms(az_str, az, 2));

    /* Calculate Solar transit info. */
    error = transit_coord(&timep, &obj[9], x_pole, y_pole, accuracy, &decr, &rar);
    if (error != 0) {
        printf_if(3, "ERRCODE %d in transit_coord.", error);
        goto out;
    }
    printf_if(1, "\nSolar transit: {%15.10f %15.10f}\n", decr, rar);
    tmp = rar - geo_loc.longitude;
    printf_if(1, "Transits observer: %15.10f degrees (%s)\n", tmp,
            as_hms(ra_str, normalize(tmp / 15.0, 24.0)));

    /* Calculate Moon phase info. */
    error = moon_phase(&timep, &obj[9], &obj[10], accuracy, &phlat, &phlon, &phindex);
    if (error != 0) {
        printf_if(3, "ERRCODE %d from moon_phase.", error);
        goto out;
    }
    if (phindex < 0 || phindex > 7) {
        printf("WARNING: Moon phase index (%d) is out of bounds.\n", phindex);
        phindex = 0;
    }
    printf_if(1, "Moon phase: {%15.10f %15.10f} %s [%+9.5f]\n", phlat, phlon,
            moon_phase_names[phindex], (double)(phindex * 45) - normalize(phlon, 360.0));

out:
    if (f_bull_a) {
        bull_a_cleanup();
    }
out2:
    return error;
}

static void usage() {
    printf("Usage:\n\n"
           "  %s [OPTIONS]\n\n"
           "Print locations of planets from an Earth observer's perspective.\n\n"
           "OPTIONS are:\n"
           "  -l NUMBER\n"
           "    Observer's latitude in degrees North of the Equator\n"
           "  -L NUMBER\n"
           "    Observer's longitude in degrees East of the Prime Meridian\n"
           "  -H NUMBER\n"
           "    Observer's height in meters above sea level\n"
           "  -T NUMBER\n"
           "    Observer's air temperature in degrees Celcius\n"
           "  -P NUMBER\n"
           "    Observer's air pressure in millibars. To convert inches to\n"
           "    millibars, multiply by 33.8637526.\n"
           "  -t TIME\n"
           "    UTC time (24-hour format) as appropriate for the system locale\n"
           "  -d DATE\n"
           "    UTC date as appropriate for the system locale\n"
           "  -A\n"
           "    Do not use Bulletin A to correct for polar motion (wobble).\n"
           "  -a\n"
           "    Express azimuth as \"degrees west of South\", rather than\n"
           "    \"degrees east of North\".\n"
           "  -s\n"
           "    Save observer parameters as defaults for future runs.\n"
           "  -v\n"
           "    Verbose output\n"
           "  -h\n"
           "    This help message\n",
        our_prog_name);
}

int main(int argc, char *argv[]) {
	char* p1 = strrchr(argv[0], '/');
	strncpy(our_prog_name, (p1 == NULL)? argv[0]: p1 + 1, sizeof(our_prog_name));
	our_prog_name[sizeof(our_prog_name) - 1] = 0;

    /*
     * Defaults.
     * These are set to some location in Seattle.
     *
     * To make your normal observation location the default, use:
     * "./planets -l <lat> -L <long> -H <height> -T <temp> -P <pressure> -s"
     */
    on_surface obs = {
        .latitude = 47.6096694,
        .longitude = -122.340412,
        .height = 10.0,
        .temperature = 15.0,
        .pressure = 1026.4
    };
    load_obs(&obs);

    struct tm utc;
    time_now_utc(&utc);
    int f_save_obs = 0;

    for (;;) {
        int c = getopt(argc, argv, ":l:L:H:T:P:t:d:vasAh");
        if (c == -1) {
            break;
        }
        switch (c) {
            case 'l':
                parse_double(optarg, &obs.latitude);
                break;
            case 'L':
                parse_double(optarg, &obs.longitude);
                break;
            case 'H':
                parse_double(optarg, &obs.height);
                break;
            case 'T':
                parse_double(optarg, &obs.temperature);
                break;
            case 'P':
                parse_double(optarg, &obs.pressure);
                break;
            case 't':
                strptime(optarg, "%X", &utc);
                break;
            case 'd':
                strptime(optarg, "%x", &utc);
                break;
            case 'a':
                f_azi_s = 1;
                break;
            case 's':
                f_save_obs = 1;
                break;
            case 'A':
                f_bull_a = 0;
                break;
            case 'v':
                ++g_verbosity;
                break;
            default:
                printf("Unknown option.\n");
                /* falls through */
            case 'h':
                usage();
                return 1;
        }
    }
    if (f_save_obs) {
        save_obs(&obs);
    }

    char workpath[PATH_MAX];
    get_ephfilename(workpath, sizeof(workpath), 1);
    g_ephfile_name = workpath;
    int res = planets_main(&obs, &utc);
    finalize_jpleph();
    return res;
}

/* vim:set ts=4 sts=4 sw=4 cindent expandtab: */
