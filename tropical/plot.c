#include "plot.h"
#include <stdio.h>
#include <ephutil.h>
#include <cairo/cairo.h>
#include <kplot.h>
#include <novas.h>
#include <float.h>

#define PLOT_WIDTH  1024
#define PLOT_HEIGHT 768

struct scale_factor {
    const char *name;
    double unit_jd;
};

static const struct scale_factor scale_factors[] = {
    { "200 ms",   2.314814814814810E-06 },
    { "1 sec",    1.157407407407410E-05 },
    { "6 sec",    6.944444444444440E-05 },
    { "30 sec",   3.472222222222220E-04 },
    { "2 min",    1.388888888888890E-03 },
    { "10 min",   6.944444444444440E-03 },
    { "45 min",   3.125000000000000E-02 },
    { "3 hr",     1.250000000000000E-01 },
    { "12 hr",    5.000000000000000E-01 },
    { "2 day",    2.000000000000000E+00 },
    { "10 day",   1.000000000000000E+01 },
    { "45 day",   4.500000000000000E+01 },
    { "0.5 yr",   1.826212500000000E+02 },
    { "2 yr",     7.304850000000000E+02 },
    { "10 yr",    3.652425000000000E+03 },
    { "50 yr",    1.826212500000000E+04 },
    { "250 yr",   9.131062500000000E+04 }
};
static const size_t nbr_of_scale_factors = NELTS(scale_factors);

/*
 * Find the largest scale_factor that is less than the actual chart range.
 */
static const struct scale_factor *get_scale_factor(double range)
{
    size_t idx;
    range /= 3.0;
    range += DBL_EPSILON;
    const struct scale_factor *ptr = &scale_factors[1];
    for (idx = 1; idx < nbr_of_scale_factors; ++idx, ++ptr) {
        if (range < ptr->unit_jd) {
            break;
        }
    }
    return &scale_factors[idx - 1];
}

struct fill_ctx {
    double lo, hi; /* left and right edges */
    double iota;   /* delta x between each successive point */
    double bias;   /* reduce x by this when reporting it to kplot */
    double phase;  /* x offset from left edge of chart to the first point */
    double max, min;
    basic_fn *fn;
};

static void
fill(size_t pos, struct kpair *val, void *dat)
{
    struct fill_ctx *ctx = (struct fill_ctx *)dat;
    double x = ctx->lo + ctx->phase + (ctx->iota * (double)pos);
    val->x = x - ctx->bias;
    double tmp = val->y = (*ctx->fn)(x);
    if (tmp < ctx->min) {
        ctx->min = tmp;
    }    
    if (tmp > ctx->max) {
        ctx->max = tmp;
    }    
}

void plot_me(double lo, double hi, size_t curve_pts, basic_fn fn)
{
    short int year, month, day;
    double hour;
    char plot_name[64];
    struct fill_ctx curve_ctx;
    struct fill_ctx pt_ctx;
    const struct scale_factor *scale = get_scale_factor(hi - lo);
    size_t pt_pts;

    pt_ctx.lo = curve_ctx.lo = lo;
    pt_ctx.hi = curve_ctx.hi = hi;
    pt_ctx.fn = curve_ctx.fn = fn;
    pt_ctx.max = curve_ctx.max = -DBL_MAX;
    pt_ctx.min = curve_ctx.min = DBL_MAX;
    curve_ctx.phase = 0.0;
    pt_ctx.bias = curve_ctx.bias = (hi + lo) * 0.5;
    curve_ctx.iota = (hi - lo) / (double)curve_pts;
    pt_ctx.iota = scale->unit_jd;

    double qu = floor(lo / scale->unit_jd);
    double pt_base = qu * scale->unit_jd;
    pt_ctx.phase = pt_base + scale->unit_jd - lo;
    pt_pts = (size_t)(floor(hi / scale->unit_jd) - qu);

    struct kdata *curve_data = kdata_array_alloc(NULL, curve_pts);
    if (curve_data == NULL) {
        perror("kdata_array_alloc curve points failed");
        return;
    }
    struct kdata *pt_data = kdata_array_alloc(NULL, pt_pts);
    if (pt_data == NULL) {
        perror("kdata_array_alloc pt points failed");
        goto out_kdata;
    }
    if ((kdata_array_fill(curve_data, &curve_ctx, fill) == 0) ||
            (kdata_array_fill(pt_data, &pt_ctx, fill) == 0)) {
        perror("kdata_array_fill failed");
        goto out_kdata;
    }
    struct kplot *p = kplot_alloc(NULL);
    if (p == NULL) {
		perror(NULL);
		goto out_ptdata;
	}
    if ((kplot_attach_data(p, curve_data, KPLOT_LINES, NULL) == 0) ||
            (kplot_attach_data(p, pt_data, KPLOT_POINTS, NULL) == 0)) {
		perror(NULL);
		goto out_kplot;
    }

    cairo_surface_t	*surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
            PLOT_WIDTH, PLOT_HEIGHT);
    cairo_status_t st = cairo_surface_status(surf);
    if (CAIRO_STATUS_SUCCESS != st) {
		fprintf(stderr, "%s", cairo_status_to_string(st));
        goto out_surf;
	}

	cairo_t *cr = cairo_create(surf);
	st = cairo_status(cr);
	if (CAIRO_STATUS_SUCCESS != st) {
		fprintf(stderr, "%s", cairo_status_to_string(st));
        goto out_surf;
	}

	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); 
	cairo_rectangle(cr, 0.0, 0.0, (double)PLOT_WIDTH, (double)PLOT_HEIGHT);
	cairo_fill(cr);
	kplot_draw(p, (double)PLOT_WIDTH, (double)PLOT_HEIGHT, cr);

    cal_date((hi + lo)/2.0, &year, &month, &day, &hour);
    snprintf(plot_name, sizeof(plot_name), "plot%4d-%02d-%02d.png", year, month, day);
    printf("chart %s: %zu points, iota %s, domain [%7.9lf, %7.9lf], range [%3.13lf, %3.13lf]\n",
            plot_name, pt_pts, scale->name, curve_ctx.lo, curve_ctx.hi, curve_ctx.min, curve_ctx.max);

	st = cairo_surface_write_to_png(cairo_get_target(cr), plot_name);
	if (CAIRO_STATUS_SUCCESS != st) {
		fprintf(stderr, "%s", cairo_status_to_string(st));
        goto out_cr;
	}

out_cr:
    cairo_destroy(cr);
out_surf:
    cairo_surface_destroy(surf);
out_kplot:
    kplot_free(p);
out_ptdata:
    kdata_destroy(pt_data);
out_kdata:
    kdata_destroy(curve_data);
}