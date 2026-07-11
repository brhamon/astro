#include "astro/frames.hpp"

#include <cmath>

// IAU 2000 nutation series + fundamental arguments + mean obliquity, ported
// from NOVAS-C (nutation.c, novas.c). The large coefficient tables are
// extracted verbatim into nutation_tables.inc by tools/extract_nutation_tables.py.

namespace astro {

namespace {

// NOVAS constants (novascon.c), used verbatim for oracle equivalence.
constexpr double kT0 = 2451545.0;
constexpr double kAsec2Rad = 4.848136811095359935899141e-6;
constexpr double kAsec360 = 1296000.0;
constexpr double kTwoPi = 6.283185307179586476925287;

#include "nutation_tables.inc"

// IAU 2000A nutation (radians). jd = jd_high + jd_low (TDB). NOVAS iau2000a.
void iau2000a(double jd_high, double jd_low, double* dpsi, double* deps) {
  const double t = ((jd_high - kT0) + jd_low) / 36525.0;

  double a[5];
  fundamental_arguments(t, a);

  // Luni-solar terms (summed in reverse, as in NOVAS, for bit-for-bit match).
  double dp = 0.0, de = 0.0;
  for (int i = 677; i >= 0; --i) {
    const double arg = std::fmod(
        nals_a[i][0] * a[0] + nals_a[i][1] * a[1] + nals_a[i][2] * a[2] +
            nals_a[i][3] * a[3] + nals_a[i][4] * a[4],
        kTwoPi);
    const double sarg = std::sin(arg), carg = std::cos(arg);
    dp += (cls_a[i][0] + cls_a[i][1] * t) * sarg + cls_a[i][2] * carg;
    de += (cls_a[i][3] + cls_a[i][4] * t) * carg + cls_a[i][5] * sarg;
  }
  const double factor = 1.0e-7 * kAsec2Rad;
  const double dpsils = dp * factor, depsls = de * factor;

  // Planetary terms. Arguments per NOVAS iau2000a (Souchay et al. 1999).
  const double al = std::fmod(2.35555598 + 8328.6914269554 * t, kTwoPi);
  const double alsu = std::fmod(6.24006013 + 628.301955 * t, kTwoPi);
  const double af = std::fmod(1.627905234 + 8433.466158131 * t, kTwoPi);
  const double ad = std::fmod(5.198466741 + 7771.3771468121 * t, kTwoPi);
  const double aom = std::fmod(2.18243920 - 33.757045 * t, kTwoPi);
  const double apa = (0.02438175 + 0.00000538691 * t) * t;
  const double alme = std::fmod(4.402608842 + 2608.7903141574 * t, kTwoPi);
  const double alve = std::fmod(3.176146697 + 1021.3285546211 * t, kTwoPi);
  const double alea = std::fmod(1.753470314 + 628.3075849991 * t, kTwoPi);
  const double alma = std::fmod(6.203480913 + 334.0612426700 * t, kTwoPi);
  const double alju = std::fmod(0.599546497 + 52.9690962641 * t, kTwoPi);
  const double alsa = std::fmod(0.874016757 + 21.3299104960 * t, kTwoPi);
  const double alur = std::fmod(5.481293871 + 7.4781598567 * t, kTwoPi);
  const double alne = std::fmod(5.321159000 + 3.8127774000 * t, kTwoPi);

  dp = 0.0;
  de = 0.0;
  for (int i = 686; i >= 0; --i) {
    const double arg = std::fmod(
        napl_a[i][0] * al + napl_a[i][1] * alsu + napl_a[i][2] * af +
            napl_a[i][3] * ad + napl_a[i][4] * aom + napl_a[i][5] * alme +
            napl_a[i][6] * alve + napl_a[i][7] * alea + napl_a[i][8] * alma +
            napl_a[i][9] * alju + napl_a[i][10] * alsa + napl_a[i][11] * alur +
            napl_a[i][12] * alne + napl_a[i][13] * apa,
        kTwoPi);
    const double sarg = std::sin(arg), carg = std::cos(arg);
    dp += cpl_a[i][0] * sarg + cpl_a[i][1] * carg;
    de += cpl_a[i][2] * sarg + cpl_a[i][3] * carg;
  }
  *dpsi = dp * factor + dpsils;
  *deps = de * factor + depsls;
}

// NU2000K truncated nutation (radians). NOVAS nu2000k.
void nu2000k(double jd_high, double jd_low, double* dpsi, double* deps) {
  const double t = ((jd_high - kT0) + jd_low) / 36525.0;

  double a[5];
  fundamental_arguments(t, a);

  double dp = 0.0, de = 0.0;
  for (int i = 322; i >= 0; --i) {
    const double arg = std::fmod(
        nals_k[i][0] * a[0] + nals_k[i][1] * a[1] + nals_k[i][2] * a[2] +
            nals_k[i][3] * a[3] + nals_k[i][4] * a[4],
        kTwoPi);
    const double sarg = std::sin(arg), carg = std::cos(arg);
    dp += (cls_k[i][0] + cls_k[i][1] * t) * sarg + cls_k[i][2] * carg;
    de += (cls_k[i][3] + cls_k[i][4] * t) * carg + cls_k[i][5] * sarg;
  }
  const double factor = 1.0e-7 * kAsec2Rad;
  const double dpsils = dp * factor, depsls = de * factor;

  // Planetary terms use the Delaunay args plus planetary longitudes
  // (Simon et al. 1994), high-order terms omitted.
  const double alme = std::fmod(4.402608842461 + 2608.790314157421 * t, kTwoPi);
  const double alve = std::fmod(3.176146696956 + 1021.328554621099 * t, kTwoPi);
  const double alea = std::fmod(1.753470459496 + 628.307584999142 * t, kTwoPi);
  const double alma = std::fmod(6.203476112911 + 334.061242669982 * t, kTwoPi);
  const double alju = std::fmod(0.599547105074 + 52.969096264064 * t, kTwoPi);
  const double alsa = std::fmod(0.874016284019 + 21.329910496032 * t, kTwoPi);
  const double alur = std::fmod(5.481293871537 + 7.478159856729 * t, kTwoPi);
  const double alne = std::fmod(5.311886286677 + 3.813303563778 * t, kTwoPi);
  const double apa = (0.024380407358 + 0.000005391235 * t) * t;

  dp = 0.0;
  de = 0.0;
  for (int i = 164; i >= 0; --i) {
    const double arg = std::fmod(
        napl_k[i][0] * a[0] + napl_k[i][1] * a[1] + napl_k[i][2] * a[2] +
            napl_k[i][3] * a[3] + napl_k[i][4] * a[4] + napl_k[i][5] * alme +
            napl_k[i][6] * alve + napl_k[i][7] * alea + napl_k[i][8] * alma +
            napl_k[i][9] * alju + napl_k[i][10] * alsa + napl_k[i][11] * alur +
            napl_k[i][12] * alne + napl_k[i][13] * apa,
        kTwoPi);
    const double sarg = std::sin(arg), carg = std::cos(arg);
    dp += cpl_k[i][0] * sarg + cpl_k[i][1] * carg;
    de += cpl_k[i][2] * sarg + cpl_k[i][3] * carg;
  }
  *dpsi = dp * factor + dpsils;
  *deps = de * factor + depsls;
}

}  // namespace

void fundamental_arguments(double t, double a[5]) {
  a[0] = std::fmod(485868.249036 +
                       t * (1717915923.2178 +
                            t * (31.8792 + t * (0.051635 + t * (-0.00024470)))),
                   kAsec360) *
         kAsec2Rad;
  a[1] = std::fmod(1287104.79305 +
                       t * (129596581.0481 +
                            t * (-0.5532 + t * (0.000136 + t * (-0.00001149)))),
                   kAsec360) *
         kAsec2Rad;
  a[2] = std::fmod(335779.526232 +
                       t * (1739527262.8478 +
                            t * (-12.7512 + t * (-0.001037 + t * (0.00000417)))),
                   kAsec360) *
         kAsec2Rad;
  a[3] = std::fmod(1072260.70369 +
                       t * (1602961601.2090 +
                            t * (-6.3706 + t * (0.006593 + t * (-0.00003169)))),
                   kAsec360) *
         kAsec2Rad;
  a[4] = std::fmod(450160.398036 +
                       t * (-6962890.5431 +
                            t * (7.4722 + t * (0.007702 + t * (-0.00005939)))),
                   kAsec360) *
         kAsec2Rad;
}

double mean_obliquity(double jd_tdb) {
  const double t = (jd_tdb - kT0) / 36525.0;
  return ((((-0.0000000434 * t - 0.000000576) * t + 0.00200340) * t -
           0.0001831) *
              t -
          46.836769) *
             t +
         84381.406;
}

void nutation_angles(double t, Accuracy accuracy, double& dpsi, double& deps) {
  const double t1 = t * 36525.0;
  double dp, de;
  if (accuracy == Accuracy::full)
    iau2000a(kT0, t1, &dp, &de);
  else
    nu2000k(kT0, t1, &dp, &de);
  dpsi = dp / kAsec2Rad;  // radians -> arcseconds
  deps = de / kAsec2Rad;
}

}  // namespace astro
