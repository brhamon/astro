# libastro

A modern **C++23** library that consumes the JPL **DE440/DE441** binary
ephemerides directly and exposes a well-typed astrometric interface — the
eventual replacement for this project's use of NOVAS-C.

Design rationale and the underlying data-model / API research live in
[`../docs/de440-and-novas-research.md`](../docs/de440-and-novas-research.md).

Learning the API:

- **[`docs/tutorial.md`](docs/tutorial.md)** — a hands-on walkthrough: open the
  ephemeris → planet positions → rise/set → seasons → apsides, in code or from
  the command line.
- **[`docs/reference.md`](docs/reference.md)** — the full public surface, layer
  by layer, with the NOVAS-C analogue for each piece.

## Status

Every implemented piece is validated **bit-for-bit against NOVAS-C** (the
generators in `test/gen/` are the oracles; see `test/`).

- **Layer 0** — `Ephemeris`: header + constant-block parse, per-body Chebyshev
  `state()` (all reconstruction paths, both unit systems, two-part JD). ✅
- **Earth orientation** — IAU 2000A / NU2000K nutation, mean obliquity,
  fundamental arguments, precession, frame tie, sidereal time. ✅
- **Layer 2 `place()`** — solar-system bodies *and* stars (proper motion /
  parallax); geocentric & surface observers; all four coordinate systems
  (`gcrs`, `astrometric`, `equator_equinox` apparent-of-date, `equator_cio`
  equator & CIO of date); full & reduced accuracy; radial velocity
  (`rad_vel`). ✅
- **`equ2hor`** — apparent RA/Dec → local zenith distance / azimuth, with polar
  motion and refraction. ✅
- **Civil time** — calendar ⇄ Julian date, leap seconds, and UTC → {TT, UT1,
  delta_t} (`astro/time.hpp`). ✅
- **Layer 3 phenomena** — derived events as lazy `std::generator` streams (not
  day-keyed tables): `tropical_moments` (equinoxes/solstices), `horizon_events`
  (rise/transit/set, with the standard horizon conventions), and `apsides`
  (perihelion/aphelion, perigee/apogee). Each runs forward or backward from a
  start time and computes only what you pull. ✅
- **`examples/planets`** — end-to-end demo taking a civil UTC date, through the
  public API (`utc_time_scales` → `place` → `equ2hor`), reproducing the legacy
  `planets` core including its Polaris line. ✅
- **`astro` CLI** — one multi-command tool exercising the whole surface (`state`,
  `place`, `sky`, `rise`, `seasons`, `apsides`, `time`, `constant`, `info`),
  built on a vendored header-only argument parser (`third_party/argparse`). ✅

`place()` covers the full NOVAS coordinate/object surface, and the phenomena
layer is in. Remaining niceties: library packaging (install / export /
`find_package`).

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

Just a C++23 toolchain and CMake — Nix is **optional**:

- A C++23 compiler: **GCC 15+**, or a recent Clang (17+) with a C++23 standard
  library. (Builds natively wherever one is installed — e.g. a system GCC 15.)
- CMake 3.28+ and a generator (Ninja or Make).
- `std::mdspan`: used via a native `<mdspan>` if the toolchain has one, else the
  vendored Kokkos reference impl (`scripts/vendor_mdspan.sh`) or, failing that,
  CMake `FetchContent` at configure time (needs network once).
- `curl` for the one-time ephemeris download.

The bundled **`flake.nix`** is a convenience: `nix develop` drops you into a
known-good toolchain (Clang 22 + GCC 15 libstdc++, CMake, Ninja, curl) so you
don't have to assemble one. It is not required to build.

## Quick start

With your own toolchain (no Nix):

```sh
./scripts/vendor_mdspan.sh          # only if <mdspan> is absent and you want to avoid FetchContent
./scripts/fetch_ephemeris.sh        # curl the DE440 binary into data/ (verifies sha256)
cmake -B build                      # add -G Ninja if you prefer Ninja
cmake --build build
ctest --test-dir build
./build/examples/planets 2025 6 21 20.0   # civil UTC date -> RA/Dec + Alt/Az table

# ...or the multi-command CLI (see docs/tutorial.md):
export LIBASTRO_EPHEMERIS=$PWD/data/JPLEPH
./build/cli/astro sky     2025-06-21T20:00 --observer 47.61,-122.33,10
./build/cli/astro seasons 2025-01-01 -n 4
./build/cli/astro apsides earth 2025-01-01 -n 2
```

Prefer the pinned toolchain? Prefix with `nix develop` (or run the above inside
`nix develop`).

Point the library at the ephemeris via the `LIBASTRO_EPHEMERIS` env var or by
passing the path to `astro::Ephemeris::open(...)`. `scripts/fetch_ephemeris.sh`
writes to `data/JPLEPH` by default.

## Verifying the results

libastro is validated against **NOVAS-C**, the U.S. Naval Observatory's reference
implementation: the test suite compares libastro's output to NOVAS-C's for
ephemeris state, the `place()` reductions (all coordinate systems, bodies and
stars), nutation, `equ2hor`, the named constants, and calendar conversions. So
"is libastro correct?" reduces to "does it match NOVAS-C?" — which you can check
two ways, depending on what you want to know. Comparisons are tolerance-based
(far below any physical significance), so platform round-off differences are
fine.

**"Does it work in my build?" — the default (no NOVAS needed).**
`ctest` replays a small committed slice of NOVAS-C reference values
(`test/vectors/golden_*.csv`) through your build. A pass means your
toolchain/platform computes the right numbers. Fetch the ephemeris first so the
state/place/star checks can run (`scripts/fetch_ephemeris.sh`); the nutation,
`equ2hor`, and time checks need nothing extra.

```sh
./scripts/fetch_ephemeris.sh
cmake -B build && cmake --build build
ctest --test-dir build          # replays the committed golden vectors
```

**"Prove it to me" — independent, full-range equivalence.**
Don't trust the committed numbers? Build NOVAS-C yourself and let CTest
regenerate the references from *your* NOVAS across the **entire** DE440 span,
then replay them. A zero residual means libastro reproduces NOVAS-C exactly on
your machine — no trust in this repo required.

```sh
( cd .. && make )               # fetches + patches + builds NOVAS-C -> novasc3.1/novas.a
cmake -B build && cmake --build build
ctest --test-dir build          # fixtures now regenerate full-range refs and replay
```

When `novas.a` is present the fixtures regenerate; otherwise the same tests fall
back to the golden slice. See `test/gen/README.md` for the oracle mechanics and
how to refresh the golden files.

## Layout

```
flake.nix                 optional pinned toolchain (nix develop); not required
CMakeLists.txt            library target `astro` + optional tests/examples/CLI
cmake/                    helper modules (mdspan resolution)
docs/                     tutorial.md, reference.md
third_party/mdspan/       vendored Kokkos std::mdspan (via scripts/vendor_mdspan.sh)
third_party/argparse/     vendored p-ranav/argparse (MIT), used by the CLI
include/astro/            public headers (Layer 0 + Layer 2 + Layer 3 + vocabulary)
src/                      implementations
examples/                 planets: end-to-end demo over the public API
cli/                      astro: multi-command CLI over the public API
tools/                    extract_nutation_tables.py (IAU 2000 series -> src/)
scripts/                  fetch_ephemeris.sh, vendor_mdspan.sh
data/                     fetched ephemeris (gitignored)
test/
  gen/                    oracle generators (gen_vectors: novas.a; gen_constants: con440.c)
  vectors/                committed golden vectors (NOVAS-free build-sanity tier)
  unit/                   unit tests; oracle CSVs regenerated into the build tree
```

Oracle CSVs are produced per selected ephemeris into `build/test/` by CTest
fixtures (`gen_state`, `gen_const`) and are not committed; see `test/gen/`.

## `import std;`

The scaffold compiles with classic `#include`s so it configures and builds
without the bleeding-edge CMake module-std path. The toolchain supports
`import std;`; flipping the project to modules is a follow-up (see the josuttis
flake for the `build-std-module` helper).
