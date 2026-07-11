# libastro

A modern **C++23** library that consumes the JPL **DE440/DE441** binary
ephemerides directly and exposes a well-typed astrometric interface — the
eventual replacement for this project's use of NOVAS-C.

Design rationale and the underlying data-model / API research live in
[`../docs/de440-and-novas-research.md`](../docs/de440-and-novas-research.md).

## Status

Every implemented piece is validated **bit-for-bit against NOVAS-C** (the
generators in `test/gen/` are the oracles; see `test/`).

- **Layer 0** — `Ephemeris`: header + constant-block parse, per-body Chebyshev
  `state()` (all reconstruction paths, both unit systems, two-part JD). ✅
- **Earth orientation** — IAU 2000A / NU2000K nutation, mean obliquity,
  fundamental arguments, precession, frame tie, sidereal time. ✅
- **Layer 2 `place()`** — solar-system bodies *and* stars (proper motion /
  parallax); geocentric & surface observers; `gcrs`, `astrometric`, and
  `equator_equinox` (apparent of date); full & reduced accuracy; radial
  velocity (`rad_vel`). ✅
- **`equ2hor`** — apparent RA/Dec → local zenith distance / azimuth, with polar
  motion and refraction. ✅
- **Civil time** — calendar ⇄ Julian date, leap seconds, and UTC → {TT, UT1,
  delta_t} (`astro/time.hpp`). ✅
- **`examples/planets`** — end-to-end demo taking a civil UTC date, through the
  public API (`utc_time_scales` → `place` → `equ2hor`), reproducing the legacy
  `planets` core including its Polaris line. ✅

Not yet: `place()` CIO system (`equator_cio`).

## Relationship to the legacy C code

This directory is **self-contained and extraction-ready**. It does *not* depend
on the legacy top-level `Makefile`. The only piece of the old
fetch/patch/build machinery reused here is the **curl download of the binary
ephemeris** (`scripts/fetch_ephemeris.sh`).

The legacy C programs and the NOVAS-C distribution remain in the parent repo as
frozen reference. NOVAS-C is used by this library **only** as a test oracle: the
generators in `test/gen/` link the legacy-built `../novasc3.1/novas.a` and emit
reference outputs. Those outputs are large and specific to the selected
ephemeris, so they are **not committed** — CTest regenerates them into the build
tree (per configured ephemeris) via setup fixtures before the tests that consume
them. The small generators are checked in instead. When NOVAS or the ephemeris
is absent the affected tests skip (exit 0), so CI without the downloads stays
green.

If libastro later earns its own release cadence, `git subtree split -P libastro`
extracts it with history into a standalone repo.

## Prerequisites

- The flake toolchain: `nix develop` (Clang 22 + GCC 15 libstdc++, CMake, Ninja, curl).
- One-time vendoring of `std::mdspan` and one-time ephemeris download.

## Quick start

```sh
nix develop
./scripts/vendor_mdspan.sh          # fetch Kokkos reference std::mdspan into third_party/
./scripts/fetch_ephemeris.sh        # curl the DE440 binary into data/ (verifies sha256)
cmake -B build -G Ninja
cmake --build build
ctest --test-dir build
./build/examples/planets 2025 6 21 20.0   # civil UTC date -> RA/Dec + Alt/Az table
```

Point the library at the ephemeris via the `LIBASTRO_EPHEMERIS` env var or by
passing the path to `astro::Ephemeris::open(...)`. `scripts/fetch_ephemeris.sh`
writes to `data/JPLEPH` by default.

## Layout

```
flake.nix                 own toolchain (does not touch the legacy build)
CMakeLists.txt            library target `astro` + optional tests
cmake/                    helper modules (mdspan resolution)
third_party/mdspan/       vendored Kokkos std::mdspan (via scripts/vendor_mdspan.sh)
include/astro/            public headers (Layer 0 + Layer 2 + vocabulary)
src/                      implementations
examples/                 planets: end-to-end demo over the public API
tools/                    extract_nutation_tables.py (IAU 2000 series -> src/)
scripts/                  fetch_ephemeris.sh, vendor_mdspan.sh
data/                     fetched ephemeris (gitignored)
test/
  gen/                    oracle generators (gen_vectors: novas.a; gen_constants: con440.c)
  vectors/                committed reference vectors only (e.g. USNO checkout files)
  unit/                   unit tests; oracle CSVs regenerated into the build tree
```

Oracle CSVs are produced per selected ephemeris into `build/test/` by CTest
fixtures (`gen_state`, `gen_const`) and are not committed; see `test/gen/`.

## `import std;`

The scaffold compiles with classic `#include`s so it configures and builds
without the bleeding-edge CMake module-std path. The toolchain supports
`import std;`; flipping the project to modules is a follow-up (see the josuttis
flake for the `build-std-module` helper).
