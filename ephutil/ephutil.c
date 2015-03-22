#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h> /* only needed for time() and localtime() calls */
#include <stdio.h>
#include <novas.h>
#include "ephutil.h"

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

const planet_t the_planets[NBR_OF_PLANETS] = {
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

double normalize(double val, double period)
{
    double quot = val / period;
    return period * (quot - floor(quot));
}

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
double leapsec_tai_utc(double jd_utc) {
    const leap_second_t* leap;

    for (leap = leap_seconds; leap[1].jd < jd_utc; ++leap)
        ;
    return leap->tai_utc;
}

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
    tp->leapsecs = leapsec_tai_utc(jd_utc);

    tp->jd_tt = tp->jd_utc + (tp->leapsecs + 32.184) / 86400.0;
    tp->jd_ut1 = tp->jd_utc + ut1_utc / 86400.0;
    tp->delta_t = 32.184 + tp->leapsecs - ut1_utc;

    printf ("TT=%15.6f UT1=%15.6f delta-T=%16.11f leapsecs=%5.1f\n\n", tp->jd_tt, 
    tp->jd_ut1, tp->delta_t, tp->leapsecs);
}

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
    tmp = floor(val);
    sec = (int)tmp;
    snprintf(buf, DMS_MAX, "%4dd%02d'%02d\"", deg, min, sec);
    return buf;
}

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
        tmp = floor(val);
        sec = (int)tmp;
        snprintf(buf, DMS_MAX, "%2dh%02dm%02ds", hour, min, sec);
    }
    return buf;
}

