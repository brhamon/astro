#if !defined(EPHUTIL_H_)
#define EPHUTIL_H_

#include <limits.h>
#include <novas.h>

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

enum {
    pln_mercury=1,
    pln_venus,
    pln_earth,
    pln_mars,
    pln_jupiter,
    pln_saturn,
    pln_uranus,
    pln_neptune,
    pln_pluto,
    pln_sun,
    pln_moon
};

typedef struct planet_t_ {
    int id;
    const char* name;
} planet_t;

typedef struct leap_second_t_ {
    int year;
    int month;
    int day;
    double jd;
    double tai_utc;
} leap_second_t;

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

#define DMS_MAX 15
#define HMS_MAX 13

/* at epoch J2000.0 */
static const double sidereal_year_in_days = 365.256363004;
static const double one_second_jd = 1.0 / 86400.0;

typedef struct {
    double jd;
    double lat;
    double lon;
} moment_t;

typedef double basic_fn(double jd);

#define NELTS(array) ((int)(sizeof(array)/sizeof(array[0])))

typedef struct {
    double lb;
    double ub;
} real_range;

typedef struct {
    double x;
    double fx;
} real_point;

typedef struct {
    real_point lb;
    real_point ub;
} real_range_point;

/*
 * obs -- observation parameters convenience structure
 */
static inline int is_neg(double a) {
    return a < 0.0;
}

static inline int signs_same(double a, double b) {
    return (is_neg(a) ^ is_neg(b)) == 0;
}

static inline int signs_diff(double a, double b) {
    return !signs_same(a, b);
}

struct tm;  // fwd ref

extern int g_verbosity;
extern char g_local_path[PATH_MAX];

extern double time_to_jd(const struct tm *bdtm);
extern int time_now_utc(struct tm *bdtm);
extern double get_jd_utc();
extern void make_time_parameters(time_parameters_t* tp, double jd_utc, double ut1_utc);
extern char *as_dms(char* buf, double val, int is_latitude);
extern char *as_hms(char* buf, double val);

extern void get_rtrim(char* out, size_t max_out, const char* in_beg, const char* in_end);
extern int parse_double(const char *str, double *val);

#define NBR_OF_PLANETS 11
extern const planet_t the_planets[NBR_OF_PLANETS];

/*
 * Read the title string that appears at the beginning of every JPL binary ephemeris
 * file.
 */
extern int get_eph_title(char* out_str, int out_len, const char* eph_name);

/* normalize a periodic value
 * return a double in the range [0.0, period) which represents
 * the "phase" of val. For example, normalize(theta, 2*PI) will
 * return the phase of theta in radians. normalize( julian_date, 7.0 )
 * will return the day-of-the-week for the given julian_date.
 */
extern double normalize(double val, double period);

extern double leapsec_tai_utc(double jd_utc);
extern void printf_if(int min_ver, const char *format, ...);
extern void make_local_path();
extern void load_obs(on_surface *obs);
extern void save_obs(const on_surface *obs);

/*
 * bracket_roots -- Divide <in_range> into <intervals>, then look for sign
 * changes in the value of <func> over the subintervals. Return the number
 * of subintervals that bracket a root of <func>. Store the first
 * <out_ranges_size> root-bracketing intervals in <out_ranges>.
 * A return value equal to out_ranges_size indicates that
 * truncation may have taken place; therefore, callers should provide
 * space for one more than the expected number of root-bracketing
 * intervals.
 */
int bracket_roots(basic_fn func, const real_range *in_range, int intervals,
        real_range out_ranges[], int out_ranges_size);
double zbrent(basic_fn func, const real_range *in_range, double tol);
double brent(double ax, double bx, double cx, basic_fn func, double tol,
        double *xmin, int f_find_min);
#endif
