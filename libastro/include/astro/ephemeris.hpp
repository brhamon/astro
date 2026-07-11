#ifndef ASTRO_EPHEMERIS_HPP
#define ASTRO_EPHEMERIS_HPP

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>

#include "astro/body.hpp"
#include "astro/constants.hpp"
#include "astro/error.hpp"
#include "astro/state_vector.hpp"
#include "astro/time_scales.hpp"

namespace astro {

// Per-group Chebyshev layout triple from the header (research doc 1.3):
// (1-based offset into the record, coefficients per component, sub-intervals).
struct GroupLayout {
  int offset = 0;
  int n_coeff = 0;
  int n_subintervals = 0;
};

// Parsed header (record 1) of a JPL binary ephemeris (research doc 1.2).
struct Header {
  std::string title;             // TTL, trimmed
  double jd_begin = 0.0;         // SS[0]
  double jd_end = 0.0;           // SS[1]
  double days_per_record = 0.0;  // SS[2]
  int denum = 0;                 // DE number
  int n_constants = 0;           // NCON
  double au_km = 0.0;            // AU
  double earth_moon_ratio = 0.0; // EMRAT
  std::size_t record_length = 0; // bytes
  std::size_t record_count = 0;  // data + header records

  // Groups 1..12 in IPT, plus librations (LPT) as group 13. Index by the raw
  // JPL group order (Mercury, Venus, EMB, Mars, ... Sun, Nutations, Librations).
  std::array<GroupLayout, 13> groups{};
};

// Layer 0: an opened DE440/DE441 file as an RAII value type. Unlike NOVAS-C's
// eph_manager (file-scope globals, one ephemeris at a time, not reentrant --
// research doc 2.4), instances are independent and self-contained.
//
// Movable, non-copyable (owns a file handle + record buffer).
class Ephemeris {
 public:
  Ephemeris(Ephemeris&&) noexcept;
  Ephemeris& operator=(Ephemeris&&) noexcept;
  Ephemeris(const Ephemeris&) = delete;
  Ephemeris& operator=(const Ephemeris&) = delete;
  ~Ephemeris();

  // Open and parse the header. Mirrors ephem_open's failure modes via EphError.
  static std::expected<Ephemeris, EphError> open(
      const std::filesystem::path& path);

  const Header& header() const noexcept;
  const Constants& constants() const noexcept;

  bool covers(TdbInstant t) const noexcept;

  // Position/velocity of `target` relative to `center` at TDB epoch `t`.
  // Reproduces planet_ephemeris, including the EMB/Moon/Earth reconstruction
  // via EMRAT (research doc 1.5). `target == center` yields the zero state.
  std::expected<StateVector, EphError> state(
      Point target, Point center, TdbInstant t, Units units = Units::au) const;

 private:
  Ephemeris();
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace astro

#endif  // ASTRO_EPHEMERIS_HPP
