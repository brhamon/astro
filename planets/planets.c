/*
 *  planets.c: Display zd/az of planets from ephemeris data
 * 
 *  Derived from software developed by:
 *
 *  U. S. Naval Observatory
 *  Astronomical Applications Dept.
 *  Washington, DC 
 *  http://www.usno.navy.mil/USNO/astronomical-applications
 */

#define USING_SOLSYSV2

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h> /* only needed for time() and localtime() calls */
#if !defined(USING_SOLSYSV2)
#include "eph_manager.h"
#endif
#include "novas.h"
#include "bull_a.h"

/*
coord_sys:
0 ... GCRS or "local GCRS"
1 ... true equator and equinox of date
2 ... true equator and CIO of date
3 ... astrometric coordinates, i.e., without light deflection or aberration.
*/

enum {
    coord_gcrs,
    coord_equ,
    coord_cio,
    coord_astro
};

#define DMS_MAX 12

char *as_dms(char* buf, double val) {
    int deg, min, sec;
    double tmp;
    int sign = 1;

    if (val < 0.0) {
        sign = -1;
        val = -val;
    }
    tmp = floor(val);
    val -= tmp;
    deg = (int)tmp * sign;
    val *= 60.0;
    tmp = floor(val);
    val -= tmp;
    min = (int)tmp;
    val *= 60.0;
    tmp = floor(val + 0.5);
    sec = (int)tmp;
    snprintf(buf, DMS_MAX, "%4dd%02d'%02d\"", deg, min, sec);
    return buf;
}

#define HMS_MAX 10

char *as_hms(char* buf, double val) {
    int hour, min, sec;
    double tmp;

    if (val < 0.0 || val >= 24.0) {
        snprintf(buf, DMS_MAX, "#ERANGE#");
    } else {
        tmp = floor(val);
        val -= tmp;
        hour = (int)tmp;
        val *= 60.0;
        tmp = floor(val);
        val -= tmp;
        min = (int)tmp;
        val *= 60.0;
        tmp = floor(val + 0.5);
        sec = (int)tmp;
        snprintf(buf, DMS_MAX, "%2dh%02dm%02ds", hour, min, sec);
    }
    return buf;
}

typedef struct planet_t_ {
    int id;
    const char* name;
} planet_t;

static const planet_t the_planets[] = {
    { 1, "Mercury" },
    { 2, "Venus" },
    { 3, "Earth" },
    { 4, "Mars" },
    { 5, "Jupiter" },
    { 6, "Saturn" },
    { 7, "Uranus" },
    { 8, "Neptune" },
    { 9, "Pluto" },
    { 10, "Sun" },
    { 11, "Moon" }
};
static const int nbr_of_planets = sizeof(the_planets)/sizeof(planet_t);

typedef struct leap_second_t_ {
    int year;
    int month;
    int day;
    double jd;
    double tai_utc;
} leap_second_t;

/*
 * The following table is published by IERS, and is accurate through 31-Dec-2015,
 * (which would be the next opportunity to add a leap second after the one scheduled
 * for 30-Jun-2015).
 *
 * VERY IMPORTANT: Add new leap second entries *before* the dummy record at the end.
 */
static const leap_second_t leap_seconds[] = {
    { 1972, 1,  1, 2441317.5,  10.0 },
    { 1972, 7,  1, 2441499.5,  11.0 },
    { 1973, 1,  1, 2441683.5,  12.0 },
    { 1974, 1,  1, 2442048.5,  13.0 },
    { 1975, 1,  1, 2442413.5,  14.0 },
    { 1976, 1,  1, 2442778.5,  15.0 },
    { 1977, 1,  1, 2443144.5,  16.0 },
    { 1978, 1,  1, 2443509.5,  17.0 },
    { 1979, 1,  1, 2443874.5,  18.0 },
    { 1980, 1,  1, 2444239.5,  19.0 },
    { 1981, 7,  1, 2444786.5,  20.0 },
    { 1982, 7,  1, 2445151.5,  21.0 },
    { 1983, 7,  1, 2445516.5,  22.0 },
    { 1985, 7,  1, 2446247.5,  23.0 },
    { 1988, 1,  1, 2447161.5,  24.0 },
    { 1990, 1,  1, 2447892.5,  25.0 },
    { 1991, 1,  1, 2448257.5,  26.0 },
    { 1992, 7,  1, 2448804.5,  27.0 },
    { 1993, 7,  1, 2449169.5,  28.0 },
    { 1994, 7,  1, 2449534.5,  29.0 },
    { 1996, 1,  1, 2450083.5,  30.0 },
    { 1997, 7,  1, 2450630.5,  31.0 },
    { 1999, 1,  1, 2451179.5,  32.0 },
    { 2006, 1,  1, 2453736.5,  33.0 },
    { 2009, 1,  1, 2454832.5,  34.0 },
    { 2012, 7,  1, 2456109.5,  35.0 },
    { 2015, 7,  1, 2457204.5,  36.0 },
    { 9999, 1,  1, 5373119.5,  36.0 },	// dummy end record
};

/*
 * Find the number of leap seconds for a given JD UTC.
 */
double tai_utc(double jd_utc) {
    const leap_second_t* leap;

    for (leap = leap_seconds; leap[1].jd < jd_utc; ++leap)
        ;
    return leap->tai_utc;
}

/*
 * While using NOVAS, you will need access to various representations
 * of time. We place all in one structure to make it easier to maintain.
 */
typedef struct time_parameters_t_ {
    double jd_utc;
    double leapsecs;
    double jd_tt;
    double jd_ut1;
    double delta_t;
} time_parameters_t;

/*
 * Obtain the JD UTC from the system clock, with accuracy of 1.0 seconds.
 */
double get_jd_utc() {
    short int year = 2000;
    short int month = 1;
    short int day = 1;
    double hour = 0.0;

    time_t now;
    struct tm bdtm;

    time(&now);
    if (gmtime_r(&now, &bdtm) != NULL) {
        year = bdtm.tm_year + 1900;
        month = bdtm.tm_mon + 1;
        day = bdtm.tm_mday;
        hour = (double)(bdtm.tm_hour * 3600 + bdtm.tm_min * 60 + bdtm.tm_sec) / 3600.0;
        printf("UTC: %02d/%02d/%04d hour=%12.6f\n", month, day, year, hour);
    } else {
        printf("Error obtaining local time.\n");
    }
    return julian_date(year,month,day,hour);
}

/*
 * Initialize the time representations in tp, using the current JD UTC and
 * instantaneous offset from UT1.
 */
void make_time_parameters(time_parameters_t* tp, double jd_utc, double ut1_utc) {
    tp->jd_utc = jd_utc;
    tp->leapsecs = tai_utc(jd_utc);

    tp->jd_tt = tp->jd_utc + (tp->leapsecs + 32.184) / 86400.0;
    tp->jd_ut1 = tp->jd_utc + ut1_utc / 86400.0;
    tp->delta_t = 32.184 + tp->leapsecs - ut1_utc;

    printf ("TT=%15.6f UT1=%15.6f delta-T=%16.11f leapsecs=%5.1f\n\n", tp->jd_tt, 
    tp->jd_ut1, tp->delta_t, tp->leapsecs);
}

/*
 * Put one star here for sanity checking. Polaris will always be within about 1
 * degree of the Celestial Intermediate Pole (CIP).
 *
 * starname[SIZE_OF_OBJ_NAME] = name of celestial object
 * catalog[SIZE_OF_CAT_NAME]  = catalog designator (e.g., HIP)
 * starnumber                 = integer identifier assigned to object
 * ra                         = ICRS right ascension (hours)
 * dec                        = ICRS declination (degrees)
 * promora                    = ICRS proper motion in right ascension
 *                              (milliarcseconds/year)
 * promodec                   = ICRS proper motion in declination
 *                              (milliarcseconds/year)
 * parallax                   = parallax (milliarcseconds)
 * radialvelocity             = radial velocity (km/s)
 */
static cat_entry polaris_cat = {"POLARIS", "HIP",   0,  2.530301028,  89.264109444,
               44.22, -11.75,  7.56, -17.4};

/*
 * Display Greenwich and local apparent sidereal time and Earth Rotation Angle.
 *
 * These values help explain the instantaneous transalation between ICRS and ITRS.
 */
short int display_rotation(time_parameters_t* tp, on_surface* lp, short int accuracy) {
    short int error;
    double last, gast, theta;
    if ((error = sidereal_time(tp->jd_ut1, 0.0, tp->delta_t, 1, 1, accuracy, &gast)) 
            != 0) {
        printf("Error %d from sidereal_time.", error);
        return error;
    }

    last = gast + lp->longitude / 15.0;
    if (last >= 24.0) {
        last -= 24.0;
    } else if (last < 0.0) {
        last += 24.0;
    }

    theta = era(tp->jd_ut1, 0.0);

    printf("Sidereal time: Greenwich=%16.11f, local=%16.11f\n", gast, last);
    printf("ERA %15.10f\n\n", theta);
    return 0;
}

#if defined(USING_SOLSYSV2)
/*
 * Read the title string that appears at the beginning of every JPL binary ephemeris
 * file.
 */
int get_eph_title(char* out_str, int out_len, const char* eph_name)
{
    ssize_t cnt;
    int fd;
    char *p = out_str;
    fd = open(eph_name, O_RDONLY);
    if (fd >= 0) {
        if ((cnt=read(fd, out_str, out_len-1)) == out_len-1) {
            for (p += cnt; p > out_str && p[-1] == ' '; --p)
                ;
        }
        close(fd);
    }
    *p = 0;
    return (int)(p - out_str);
}
#endif

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

/* normalize a periodic value
 * return a double in the range [0.0, period) which represents
 * the "phase" of val. For example, normalize(theta, 2*PI) will 
 * return the phase of theta in radians. normalize( julian_date, 7.0 )
 * will return the day-of-the-week for the given julian_date.
 */
double normalize(double val, double period)
{
    double quot = val / period;
    return period * (quot - floor(quot));
}

int main(void) {
/*
 * The following three values are Earth Orientation (EO) paramters published by IERS.
 * See bull_a.c for more information.
 */
    double x_pole = 0.005165;
    double y_pole = 0.366089;
    double ut1_utc = -0.5340900;

    const short int accuracy = 0;
    short int error = 0;
#if !defined(USING_SOLSYSV2)
    short int de_num = 0;
#endif

    /* Some location in Seattle. REPLACE WITH YOUR LOCATION. */
    const double latitude = 47.6096694;
    const double longitude = -122.340412;
    const double height = 10.0;
    const double temperature = 15.0;
    const double pressure = 1026.4;

    time_parameters_t timep;
    double zd, az, rar, decr, tmp; 
    on_surface geo_loc;
    observer obs_loc;
    cat_entry dummy_star;
    object obj[nbr_of_planets];
    object polaris;
    sky_pos t_place;
    int index;

    char dec_str[DMS_MAX];
    char zd_str[DMS_MAX];
    char ra_str[HMS_MAX];
    char az_str[DMS_MAX];
#if defined(USING_SOLSYSV2)
    char ttl[85];
#endif

    if ((error = bull_a_init()) != 0) {
        printf("Error %d from bull_a_init.", error);
        return error;
    }
    make_on_surface(latitude, longitude, height, temperature, pressure, &geo_loc);
    make_observer_on_surface(latitude, longitude, height, temperature, pressure,
        &obs_loc);

    make_cat_entry("DUMMY","xxx", 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, &dummy_star);

    for (index=0; index < nbr_of_planets; ++index) {
        if ((error = make_object(0, the_planets[index].id, (char*)the_planets[index].name,
                        &dummy_star, &obj[index])) != 0) {
            printf ("Error %d from make_object (%s)\n", error, the_planets[index].name);
            return error;
        }
    }
    if ((error = make_object(2, 0, "Polaris", &polaris_cat, &polaris)) != 0) {
        printf ("Error %d from make_object (%s)\n", error, polaris_cat.starname);
        return error;
    }

    printf ("PLANETS: display positions from an observer on Earth's surface\n"
        "--------------------------------------------------------------\n\n");

#if !defined(USING_SOLSYSV2)
    /*
     * Open the JPL binary ephemeris file, here named "JPLEPH".
     */

    if ((error = ephem_open("JPLEPH", &jd_beg, &jd_end, &de_num)) != 0) {
    if (error == 1) {
        printf("JPL ephemeris file not found.\n");
    } else {
        printf("Error reading JPL ephemeris file header.\n");
        return error;
    } else {
        printf("JPL ephemeris DE%d open. Start JD = %10.2f  End JD = %10.2f\n\n",
            de_num, jd_beg, jd_end);
    }
#else
    get_eph_title(ttl, sizeof(ttl), "JPLEPH");

    printf ("Using solsys version 2\n%s\n\n", ttl);
#endif

    printf("Observer geodetic location (ITRS = WGS-84):\n"
            "{ %s, %s, %6.1f } or { %15.10f %15.10f }\n\n", 
           as_dms(zd_str, geo_loc.latitude), as_dms(az_str, geo_loc.longitude), 
           geo_loc.height, geo_loc.latitude, geo_loc.longitude);

    tmp = get_jd_utc();
    bull_a_entry_t* ba_ent = bull_a_find(tmp - 2400000.5);
    if (ba_ent != NULL) {
        printf("Bulletin A: %d/%02d/%d %8.2f q=%c (%9.6f, %9.6f) %10.7f\n\n", 
                ba_ent->month, ba_ent->day, ba_ent->year, ba_ent->mjd, 
                ba_ent->pm_quality, ba_ent->pm_x, ba_ent->pm_y, ba_ent->ut1_utc);
        x_pole = ba_ent->pm_x;
        y_pole = ba_ent->pm_y;
        ut1_utc = ba_ent->ut1_utc;
    }
    make_time_parameters(&timep, tmp, ut1_utc);
    display_rotation(&timep, &geo_loc, accuracy);

    printf("%8s %15s %15s %15s %15s %15s\n", "Object", "RA", "DEC", "DIST", "ZA", 
            "AZ");
    for (index=0; index < nbr_of_planets; ++index) {
        if (index == 2) {
            continue; // skip Earth
        }
        if ((error = place(timep.jd_tt, &obj[index], &obs_loc, timep.delta_t, coord_equ, 
                        accuracy, &t_place)) != 0) {
            printf("Error %d from place.", error);
            return error;
        }

        /*
         * Position of the planet in local horizon coordinates. Correct for refraction.
         */
        equ2hor(timep.jd_ut1, timep.delta_t, accuracy, x_pole, y_pole, &geo_loc, 
                t_place.ra, t_place.dec, 2, &zd, &az, &rar, &decr);
        az -= 180.0;
        if (az < 0.0) {
            az += 360.0;
        }
        /* ZA (zenith altitude) is degrees above horizon.
         * ZD (zenith distance) is degrees below zenith. */
        zd = 90.0 - zd; // convert ZD to ZA
        printf("%8s %15s %15s %15.12f %15s %15s\n", 
               obj[index].name, as_hms(ra_str, t_place.ra), 
               as_dms(dec_str, t_place.dec), t_place.dis, as_dms(zd_str, zd), 
               as_dms(az_str, az));
    }

    if ((error = place(timep.jd_tt, &polaris, &obs_loc, timep.delta_t, coord_equ, 
                    accuracy, &t_place)) != 0) {
        printf("Error %d from place.", error);
        return error;
    }
    equ2hor(timep.jd_ut1, timep.delta_t, accuracy, x_pole, y_pole, &geo_loc,
            t_place.ra, t_place.dec, 2, &zd, &az, &rar, &decr);
    printf("%8s %15s %15s %15.12f %15s %15s\n",
            polaris_cat.starname, as_hms(ra_str, t_place.ra),
            as_dms(dec_str, t_place.dec), t_place.dis, as_dms(zd_str, zd),
            as_dms(az_str, az));

    if ((error = transit_coord(&timep, &obj[9], x_pole, y_pole, accuracy, &decr, &rar)) 
            != 0) {
        printf("Error %d in transit_coord.", error);
        return error;
    }
    printf("\nSolar transit: {%15.10f %15.10f}\n", decr, rar);
    tmp = rar - geo_loc.longitude;
    printf("Transits observer: %15.10f degrees (%s)\n", tmp,
            as_hms(ra_str, normalize(tmp / 15.0, 24.0)));

#if !defined(USING_SOLSYSV2)
    ephem_close();  /* remove this line for use with solsys version 2 */
#endif
    bull_a_cleanup();

    return 0;
}
/* vim:set ts=4 sts=4 sw=4 cindent expandtab: */
