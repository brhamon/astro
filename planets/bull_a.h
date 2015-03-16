#if !defined(BULL_A_H_)
#define BULL_A_H 1

typedef struct {
    int year;
    int month;
    int day;
    double mjd;
    char pm_quality;
    double pm_x;        /* Polar Motion 'x' value in seconds of arc. */
    double pm_x_err;    /* Polar Motion 'x' error in seconds of arc. */
    double pm_y;        /* Polar Motion 'y' value in seconds of arc. */
    double pm_y_err;    /* Polar Motion 'y' error in seconds of arc. */
    char ut1_utc_quality;
    double ut1_utc;     /* UT1-UTC value in seconds of time. */
    double ut1_utc_err; /* UT1-UTC error in seconds of time. */
} bull_a_entry_t;

int bull_a_init();
void bull_a_cleanup();
bull_a_entry_t* bull_a_find(double mjd);

#endif

/* vim:set ts=4 sts=4 sw=4 cindent expandtab: */
