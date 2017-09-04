#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "ephutil.h"

int g_verbosity = 0;

void printf_if(int min_ver, const char *format, ...)
{
    va_list ap;
    if (g_verbosity >= min_ver) {
        va_start(ap, format);
        vprintf(format, ap);
        va_end(ap);
    }
}

char *as_dms(char* buf, double val, int is_latitude) {
    static const char *direction[] = { "EW", "NS" };
    int deg, min;
    double tmp;
    int sign = 1;
    char dbuf[8];

    if (val < 0.0) {
        sign = -1;
        val = -val;
    }
    tmp = floor(val);
    val -= tmp;
    deg = (int)tmp;
    val *= 60.0;
    tmp = floor(val);
    val -= tmp;
    min = (int)tmp;
    val *= 60.0;
    if (is_latitude < 0 || is_latitude > 1) {
        snprintf(dbuf, sizeof(dbuf), "%s%d", sign < 0 ? "-" : "", deg);
        snprintf(buf, 16, "%4s\u00b0%02d'%04.1lf\"", dbuf, min, val);
    } else {
        snprintf(buf, 16, "%d\u00b0%02d'%04.1lf\"%c", deg, min, val,
                direction[is_latitude][sign < 0 ? 1: 0]);
    }
    return buf;
}

char *as_hms(char* buf, double val) {
    int hour, min;
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
        snprintf(buf, DMS_MAX, "%2dh%02dm%05.2fs", hour, min, val);
    }
    return buf;
}

void get_rtrim(char* out, size_t max_out, const char* in_beg, const char* in_end) {
    while (in_end > in_beg && in_end[-1] == ' ') {
    --in_end;
    }
    if (in_end > in_beg) {
    size_t len = in_end - in_beg;
    if (len > max_out) {
        len = max_out;
    }
    memcpy(out, in_beg, len);
    }
}

int get_int(const char* cp, size_t len) {
    char* p_end;
    long int val = 0;
    char ch = cp[len];
    ((char*)cp)[len] = '\0';
    val = strtol(cp, &p_end, 10);
    ((char*)cp)[len] = ch;
    return val;
}

double get_double(const char* cp, size_t len) {
    char* p_end;
    double val = 0.0;
    char ch = cp[len];
    ((char*)cp)[len] = '\0';
    val = strtod(cp, &p_end);
    ((char*)cp)[len] = ch;
    return val;
}

int parse_double(const char *str, double *val) {
    char *eptr;
    *val = strtod(str, &eptr);
    return *eptr == 0;
}

