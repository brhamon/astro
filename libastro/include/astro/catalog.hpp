#ifndef ASTRO_CATALOG_HPP
#define ASTRO_CATALOG_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <unordered_map>
#include <vector>

#include "astro/error.hpp"
#include "astro/reductions.hpp"  // Star

// A star catalogue: a queryable, in-memory collection of catalogued stars whose
// astrometry feeds directly into place(). The first loader targets the
// Hipparcos Main Catalogue (ESA 1997, CDS I/239); Tycho-2 / Gaia could follow.

namespace astro {

// One catalogued star: the astrometric data place() consumes, plus identity and
// brightness metadata. The Hipparcos re-reduction (I/311) is an astrometric
// catalogue: it carries the Hipparcos Hp magnitude and colours, but no Johnson V
// or spectral type (Hp is a broad passband, within a few tenths of V).
struct CatalogStar {
  Star astrometry{};       // ICRS, epoch J2000.0 -- pass straight to place()
  int hip = 0;             // Hipparcos number
  float hp_mag = std::numeric_limits<float>::quiet_NaN();  // Hipparcos Hp mag
  float bv = std::numeric_limits<float>::quiet_NaN();      // B-V colour
  float vi = std::numeric_limits<float>::quiet_NaN();      // V-I colour
};

class StarCatalog {
 public:
  // Load the Hipparcos catalogue, van Leeuwen 2007 re-reduction (CDS I/311,
  // hip2.dat). Positions (radians at the catalogue epoch J1991.25) are converted
  // to degrees and propagated to J2000.0 via proper motion, so
  // CatalogStar::astrometry is a J2000.0 entry ready for place().
  static std::expected<StarCatalog, EphError> load_hipparcos(
      const std::filesystem::path& path);

  std::size_t size() const noexcept { return stars_.size(); }
  bool empty() const noexcept { return stars_.empty(); }
  const std::vector<CatalogStar>& stars() const noexcept { return stars_; }

  // Look up by Hipparcos number; nullptr if not present.
  const CatalogStar* by_hip(int hip) const;

 private:
  std::vector<CatalogStar> stars_;
  std::unordered_map<int, std::size_t> by_hip_;  // HIP -> index into stars_
};

}  // namespace astro

#endif  // ASTRO_CATALOG_HPP
