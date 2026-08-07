#include "astro/catalog.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

// Hipparcos catalogue, van Leeuwen 2007 re-reduction (CDS I/311). The file
// hip2.dat is fixed-width ASCII, 117955 records; byte columns from its ReadMe
// (1-based):
//
//     1-  6  HIP     identifier         52- 59  pmRA   mu_a*cos(d) (mas/yr)
//    16- 28  RArad   ICRS rad, J1991.25  61- 68  pmDE   mu_delta    (mas/yr)
//    30- 42  DErad   ICRS rad, J1991.25 130-136  Hpmag  Hipparcos magnitude
//    44- 50  Plx     parallax (mas)     153-158  B-V    colour
//                                       166-171  V-I    colour
//
// Positions are at epoch J1991.25; place() expects a J2000.0 catalogue entry, so
// each position is propagated by proper motion over the 8.75-year difference
// (linear on the sky -- sub-arcsecond even for high-proper-motion stars over so
// short a baseline).

namespace astro {

namespace {

constexpr double kEpochShiftYears = 2000.0 - 1991.25;  // J1991.25 -> J2000.0
constexpr double kMasPerDegree = 3'600'000.0;
constexpr double kDeg2Rad = 0.017453292519943296;
constexpr double kRad2Deg = 57.295779513082320876798;

// A 1-based, inclusive [first,last] column slice, trimmed of blanks. Returns an
// empty view if the line is too short or the field is all blanks.
std::string_view slice(std::string_view line, std::size_t first, std::size_t last) {
  if (line.size() < last) last = line.size();
  if (first == 0 || first > last) return {};
  std::string_view v = line.substr(first - 1, last - first + 1);
  while (!v.empty() && v.front() == ' ') v.remove_prefix(1);
  while (!v.empty() && v.back() == ' ') v.remove_suffix(1);
  return v;
}

// Parse a trimmed field as a double; nullopt if blank/unparseable. from_chars
// rejects a leading '+', which the catalogue writes on positive values.
std::optional<double> as_double(std::string_view v) {
  if (!v.empty() && v.front() == '+') v.remove_prefix(1);
  if (v.empty()) return std::nullopt;
  double out = 0.0;
  const auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), out);
  if (ec != std::errc{} || ptr != v.data() + v.size()) return std::nullopt;
  return out;
}

std::optional<int> as_int(std::string_view v) {
  if (v.empty()) return std::nullopt;
  int out = 0;
  const auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), out);
  if (ec != std::errc{} || ptr != v.data() + v.size()) return std::nullopt;
  return out;
}

}  // namespace

const CatalogStar* StarCatalog::by_hip(int hip) const {
  if (auto it = by_hip_.find(hip); it != by_hip_.end()) return &stars_[it->second];
  return nullptr;
}

std::expected<StarCatalog, EphError> StarCatalog::load_hipparcos(
    const std::filesystem::path& path) {
  std::ifstream in(path);
  if (!in) return std::unexpected(EphError::file_not_found);

  StarCatalog cat;
  std::string line;
  while (std::getline(in, line)) {
    const std::string_view sv = line;
    const auto hip = as_int(slice(sv, 1, 6));
    const auto ra_rad = as_double(slice(sv, 16, 28));   // ICRS rad, J1991.25
    const auto dec_rad = as_double(slice(sv, 30, 42));
    if (!hip || !ra_rad || !dec_rad) continue;

    const double ra = *ra_rad * kRad2Deg;   // degrees, J1991.25
    const double dec = *dec_rad * kRad2Deg;
    const double pm_ra = as_double(slice(sv, 52, 59)).value_or(0.0);   // mu_a*cos d
    const double pm_dec = as_double(slice(sv, 61, 68)).value_or(0.0);
    const double plx = as_double(slice(sv, 44, 50)).value_or(0.0);

    // Propagate the position from J1991.25 to J2000.0 by proper motion.
    const double cos_dec = std::cos(dec * kDeg2Rad);
    const double ra_j2000 =
        ra + (cos_dec != 0.0 ? pm_ra * kEpochShiftYears / cos_dec / kMasPerDegree
                             : 0.0);
    const double dec_j2000 = dec + pm_dec * kEpochShiftYears / kMasPerDegree;

    CatalogStar star;
    star.hip = *hip;
    star.astrometry.ra_hours = ra_j2000 / 15.0;
    star.astrometry.dec_deg = dec_j2000;
    star.astrometry.pm_ra_mas_yr = pm_ra;
    star.astrometry.pm_dec_mas_yr = pm_dec;
    star.astrometry.parallax_mas = plx;
    star.astrometry.radial_velocity_km_s = 0.0;  // Hipparcos has no RV
    if (auto v = as_double(slice(sv, 130, 136))) star.hp_mag = static_cast<float>(*v);
    if (auto v = as_double(slice(sv, 153, 158))) star.bv = static_cast<float>(*v);
    if (auto v = as_double(slice(sv, 166, 171))) star.vi = static_cast<float>(*v);

    cat.by_hip_.emplace(*hip, cat.stars_.size());
    cat.stars_.push_back(std::move(star));
  }

  if (cat.stars_.empty()) return std::unexpected(EphError::bad_header);
  return cat;
}

}  // namespace astro
