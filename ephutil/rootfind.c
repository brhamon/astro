#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "ephutil.h"

/*
 * bracket_roots -- inward root bracketing
 *
 * Given <func>, defined over <in_range>, divide <in_range> into <intervals>
 * subranges, and if the subrange brackets a root, add it to out_ranges.
 * The maximum number of subranges to report is determined by
 * <out_ranges_size>.
 *
 * This function is derived from zbrak from NR.
 *
 * Copyright (c) 1987, 1988, 1992, 1997 by Numerical Recipies Software.
 */
int bracket_roots(basic_fn func, const real_range *in_range, int intervals,
        real_range out_ranges[], int out_ranges_size)
{
    int nbr_of_ranges;
    double delta_x;
    real_range *orr = &out_ranges[0];
    real_range_point range;

    nbr_of_ranges=0;
    delta_x = (in_range->ub - in_range->lb) / intervals;
    range.ub.x = in_range->lb;
    range.ub.fx = func(range.ub.x);
    while (range.ub.x < in_range->ub) {
        range.lb = range.ub;
        range.ub.x += delta_x;
        range.ub.fx = func(range.ub.x);
        if (signs_diff(range.ub.fx, range.lb.fx)) {
            if (nbr_of_ranges < out_ranges_size) {
                orr->lb = range.lb.x;
                orr->ub = range.ub.x;
                ++orr;
            }
            ++nbr_of_ranges;
        }
    }
    return nbr_of_ranges;
}

#define ITMAX 100
#define EPS 3.0e-8
#define ZEPS 1.0e-10
#define CGOLD 0.3819660

/*
 * zbrent -- implementation of Brent's method to find a root
 *
 * Find the root of <func> in the range <in_range>. The returned root
 * will be refined until its accuracy is <tol>.
 *
 * Copyright (c) 1987, 1988, 1992, 1997 by Numerical Recipies Software.
 */
double zbrent(basic_fn func, const real_range *in_range, double tol)
{
    int iter;
    double a, b, c, d, e, min1, min2;
    double fa, fb, fc, p, q, r, s, tol1, xm;

    a = in_range->lb;
    c = b = in_range->ub;
    fa = func(a);
    fb = func(b);
    if (signs_same(fa, fb) || fa == 0.0 || fb == 0.0) {
        printf("%s: ERROR root must be bracketed", __func__);
        exit(1);
        return 0.0;
    }
    fc = fb;
    for (iter=0; iter < ITMAX; ++iter) {
        if (signs_same(fb, fc) && fb != 0.0 && fc != 0.0) {
            c = a;
            fc = fa;
            e = d = b - a;
        }
        if (fabs(fc) < fabs(fb)) {
            a = b;
            b = c;
            c = a;
            fa = fb;
            fb = fc;
            fc = fa;
        }
        tol1 = 2.0 * EPS * fabs(b) + 0.5 * tol;
        xm = 0.5 * (c - b);
        if (fabs(xm) <= tol1 || fb == 0.0) {
            return b;
        }
        if (fabs(e) >= tol1 && fabs(fa) > fabs(fb)) {
            s=fb/fa;
            if (a == c) {
                p = 2.0 * xm * s;
                q = 1.0 - s;
            } else {
                q = fa / fc;
                r = fb / fc;
                p = s * (2.0 * xm * q * (q - r) - (b - a) * (r - 1.0));
                q = (q - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0.0) {
                q = -q;
            }
            p = fabs(p);
            min1 = 3.0 * xm * q - fabs(tol1 * q);
            min2 = fabs(e * q);
            if (2.0 * p < (min1 < min2 ? min1 : min2)) {
                e = d;
                d = p / q;
            } else {
                d = xm;
                e = d;
            }
        } else {
            d = xm;
            e = d;
        }
        a = b;
        fa = fb;
        if (fabs(d) > tol1) {
            b += d;
        } else {
            b += xm >= 0.0 ? fabs(tol1) : -fabs(tol1);
        }
        fb = func(b);
    }
    printf("%s: ERROR maximum number of iterations exceeded", __func__);
    exit(1);
    return 0.0;
}

/*
 * zbrent -- implementation of Brent's method to find a local min/max
 *
 * Given a function <func> defined over the range [ax, cx], and <bx>, a value
 * within the range where func(bx) is less than/greater than both func(ax)
 * and func(cx), isolate the minium/maximum within tolerance <tol> using
 * Brent's method. The abcissa of the mimum is stored in <xmin>.
 * The function returns func(*xmin).
 *
 * Copyright (c) 1987, 1988, 1992, 1997 by Numerical Recipies Software.
 */
double brent(double ax, double bx, double cx, basic_fn func, double tol,
        double *xmin, int f_find_min)
{
    int iter;
    double a, b, d, etemp, fu, fv, fw, fx, p, q, r, tol1, tol2, u, v, w, x, xm;
    double e = 0.0;
    a=(ax < cx) ? ax: cx;
    b=(ax > cx) ? ax: cx;
    x = w = v = bx;
    fw = fv = fx = (f_find_min ? func(x) : -(func(x)));
    for (iter=0; iter < ITMAX; ++iter) {
        xm = 0.5 * (a + b);
        tol1 = tol * fabs(x) + ZEPS;
        tol2 = 2.0 * tol1;
        if (fabs(x - xm) <= (tol2 - 0.5 * (b - a))) {
            *xmin = x;
            return f_find_min ? fx : -fx;
        }
        if (fabs(e) > tol1) {
            r = (x - w) * (fx - fv);
            q = (x - v) * (fx - fw);
            p = (x - v) * q - (x - w) * r;
            q = 2.0 * (q - r);
            if (q > 0.0) {
                p = -p;
            }
            q = fabs(q);
            etemp = e;
            e = d;
            if (fabs(p) >= fabs(0.5 * q * etemp) || p <= q * (a - x) ||
                    p >= q * (b - x)) {
                e = (x >= xm) ? a - x : b - x;
                d = CGOLD * e;
            } else {
                d = p / q;
                u = x + d;
                if (u - a < tol2 || b - u < tol2) {
                    d=(xm - x) >= 0.0 ? fabs(tol1) : -fabs(tol1);
                }
            }
        } else {
            e = (x >= xm) ? a - x : b - x;
            d = CGOLD * e;
        }
        u = (fabs(d) >= tol1 ? x + d : x + (d >= 0.0 ? fabs(tol1) : -fabs(tol1)));
        fu = f_find_min ? func(u) : -(func(u));
        if (fu <= fx) {
            if (u >= x) {
                a = x;
            } else {
                b = x;
            }
            v = w;
            fv = fw;
            w = x;
            fw = fx;
            x = u;
            fx = fu;
        } else {
            if (u < x) {
                a = u;
            } else {
                b = u;
            }
            if (fu <= fw || w == x) {
                v = w;
                fv = fw;
                w = u;
                fw = fu;
            } else if (fu <= fv || v == x || v == w) {
                v = u;
                fv = fu;
            }
        }
    }
    printf("%s: ERROR maximum number of iterations exceeded", __func__);
    *xmin = x;
    return f_find_min ? fx : -fx;
}

