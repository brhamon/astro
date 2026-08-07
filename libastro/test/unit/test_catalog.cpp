// Validate the Hipparcos catalogue loader (I/311, 2007 re-reduction) by
// self-consistency + known values (there is no NOVAS oracle for a catalogue).
// Checks the record count, the HIP-number index, and that Sirius (HIP 32349)
// parses with its known Hp magnitude, parallax, and -- crucially -- a position
// propagated to J2000.0 (the loader must convert radians and shift the J1991.25
// epoch). Needs data/hip2.dat via LIBASTRO_CATALOG (skips, exit 0, if absent).

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "astro/catalog.hpp"

namespace {
int g_fail = 0;
#define CHECK(c)                                                          \
  do {                                                                    \
    if (!(c)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++g_fail; } \
  } while (0)
}  // namespace

int main() {
  // IAU name -> HIP crosswalk: a bundled table, so this needs no data file.
  CHECK(astro::hip_for_name("Vega") == 91262);
  CHECK(astro::hip_for_name("vega") == 91262);          // case-insensitive
  CHECK(astro::hip_for_name("  Sirius ") == 32349);     // trimmed
  CHECK(astro::hip_for_name("Polaris") == 11767);
  CHECK(astro::hip_for_name("Rigil Kentaurus") == 71683);  // multi-word name
  CHECK(!astro::hip_for_name("Nonesuch").has_value());

  // Reverse lookup returns the canonical IAU name (or nothing for unnamed HIPs).
  CHECK(astro::name_for_hip(91262) == "Vega");
  CHECK(astro::name_for_hip(54061) == "Dubhe");
  CHECK(!astro::name_for_hip(2).has_value());

  const char* path = std::getenv("LIBASTRO_CATALOG");
  if (!path) {
    std::fprintf(stderr, "SKIP catalog file (LIBASTRO_CATALOG unset); "
                         "name crosswalk: %d failures\n", g_fail);
    return g_fail == 0 ? 0 : 1;
  }
  auto cat = astro::StarCatalog::load_hipparcos(path);
  if (!cat) { std::fprintf(stderr, "FAIL catalog: load\n"); return 1; }

  // The re-reduction has 117955 astrometric solutions.
  CHECK(cat->size() > 117900 && cat->size() <= 117955);

  const astro::CatalogStar* sirius = cat->by_hip(32349);
  CHECK(sirius != nullptr);
  if (sirius) {
    CHECK(sirius->hip == 32349);
    CHECK(std::fabs(sirius->hp_mag - (-1.09)) < 0.02);   // Hp of the brightest star
    CHECK(std::fabs(sirius->astrometry.parallax_mas - 379.21) < 0.5);

    // Position must be at J2000.0 (converted from radians + propagated from
    // J1991.25). Sirius J2000: RA 06h45m08.9s = 6.752478 h, Dec -16.71617 deg.
    CHECK(std::fabs(sirius->astrometry.ra_hours - 6.752478) < 1e-4);
    CHECK(std::fabs(sirius->astrometry.dec_deg - (-16.71617)) < 1e-3);
    // High proper motion is preserved.
    CHECK(std::fabs(sirius->astrometry.pm_ra_mas_yr - (-546.01)) < 0.5);
    CHECK(std::fabs(sirius->astrometry.pm_dec_mas_yr - (-1223.07)) < 0.5);

    // Sanity: the J2000 position differs from the raw J1991.25 value by the
    // ~8.75 yr proper-motion shift (a few arcsec), i.e. it was NOT left as-is.
    const double raw_ra_h = 1.7678185359 * 57.29577951308232 / 15.0;  // J1991.25
    CHECK(std::fabs(sirius->astrometry.ra_hours - raw_ra_h) > 1e-6);
  }

  // Index is consistent and rejects unknown numbers.
  CHECK(cat->by_hip(-1) == nullptr);
  const astro::CatalogStar* vega = cat->by_hip(91262);   // Vega
  CHECK(vega != nullptr);
  if (vega) CHECK(std::fabs(vega->hp_mag - 0.087) < 0.02);

  // by_name resolves through the crosswalk to the same record.
  CHECK(cat->by_name("vega") == vega);
  CHECK(cat->by_name("Betelgeuse") != nullptr);
  CHECK(cat->by_name("Nonesuch") == nullptr);

  std::fprintf(stderr, "catalog: %zu stars; Sirius/Vega OK; %d failures\n",
               cat->size(), g_fail);
  return g_fail == 0 ? 0 : 1;
}
