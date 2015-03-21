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

#if !defined(USING_SOLSYSV2)
#include "eph_manager.h"
#endif
#include <novas.h>
#include <ephutil.h>
#include "bull_a.h"

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
    object obj[NBR_OF_PLANETS];
    object polaris;
    sky_pos t_place;
    int index, earth_index=-1;

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

    for (index=0; index < NBR_OF_PLANETS; ++index) {
        if (the_planets[index].id == 3) {
            earth_index = index;
        }
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
    for (index=0; index < NBR_OF_PLANETS; ++index) {
        if (index == earth_index) {
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
