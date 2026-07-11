#ifndef ASTRO_TIME_SCALES_HPP
#define ASTRO_TIME_SCALES_HPP

// Strongly-typed time scales. NOVAS-C passes a bare `double[2]` split Julian
// date and relies on convention to distinguish TT / TDB / UT1 -- a silent bug
// source (research doc Part 2.4). Here each scale is a distinct type and the
// two-part JD is preserved for interpolation accuracy (research doc 1.4).

namespace astro {

// Two-part Julian date: value == whole + frac. Keeping the split defers
// catastrophic cancellation; matches eph_manager's jed[2] contract.
struct JulianDate {
  double whole = 0.0;
  double frac = 0.0;

  constexpr JulianDate() = default;
  constexpr explicit JulianDate(double jd) : whole(jd), frac(0.0) {}
  constexpr JulianDate(double w, double f) : whole(w), frac(f) {}

  constexpr double value() const noexcept { return whole + frac; }
};

// Barycentric Dynamical Time -- the scale the ephemeris is expressed in.
struct TdbInstant {
  JulianDate jd{};
};

// Terrestrial Time.
struct TtInstant {
  JulianDate jd{};
};

// Universal Time (UT1).
struct Ut1Instant {
  JulianDate jd{};
};

// delta_t = TT - UT1, in seconds (as used by NOVAS reductions).
struct DeltaT {
  double seconds = 0.0;
};

// TODO(layer0/vocabulary): conversions between scales.
//   TtInstant  to_tt(TdbInstant);         // NOVAS tdb2tt
//   TdbInstant to_tdb(TtInstant);
//   Ut1Instant to_ut1(TtInstant, DeltaT);
// For sub-microarcsecond work TDB<->TT needs the periodic terms; the constant
// 32.184 s offset (as in the legacy ephutil time model) is the first cut.

}  // namespace astro

#endif  // ASTRO_TIME_SCALES_HPP
