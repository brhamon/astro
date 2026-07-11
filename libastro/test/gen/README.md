# Test-vector generation

`gen_vectors.c` is the **oracle**: it links the legacy NOVAS-C archive and emits
reference `planet_ephemeris()` outputs that the C++ Layer-0 tests replay.
`gen_place.c` does the same for the Layer-2 `place()` pipeline, and
`gen_constants.c` is an oracle for the named constants.

The oracle CSVs are **not committed**: they are large and specific to the
selected ephemeris (JPL revises the DE series periodically). Instead the
generators are checked in and CTest regenerates the CSVs, from the currently
configured ephemeris, into the build tree before the tests that consume them:

```
gen_state    (fixture) --> build/test/state_vectors.csv  --> replay
gen_place    (fixture) --> build/test/place.csv          --> place
gen_star     (fixture) --> build/test/star.csv           --> star
gen_nutation (fixture) --> build/test/nutation.csv       --> nutation
gen_hor      (fixture) --> build/test/hor.csv            --> hor
gen_time     (fixture) --> build/test/time.csv           --> time
gen_const    (fixture) --> build/test/constants_ref.csv  --> constants
```

`gen_nutation`, `gen_hor`, and `gen_time` need no ephemeris (nutation/obliquity,
equ2hor, and calendar<->JD are pure functions of time + geometry); their
fixtures are wired whenever the NOVAS oracle is built. `test_time` also runs a
set of NOVAS-free self-checks (leap seconds, delta_t algebra), so it is useful
even without the fixture.

So the normal workflow is just `ctest` — no manual generation, no `git add`.

## Golden vectors (committed, NOVAS-free)

When NOVAS is *not* built, the numeric tests can't regenerate, so they instead
replay a small committed slice of NOVAS-C reference values in `../vectors/`:

```
golden_state_de440.csv   golden_place_de440.csv   golden_star_de440.csv
golden_nutation.csv      golden_hor.csv
```

These let anyone run `ctest` and confirm their build computes the right numbers
without touching NOVAS (build-sanity tier in the top-level README). They are a
representative slice — small on purpose — not the full-range regeneration. The
`_de440` files are DE440-specific; `golden_nutation`/`golden_hor` are pure
functions of time/geometry.

Refresh them only when the models or the reference change, from a NOVAS build
(sampling header + every Kth row to stay small):

```sh
cmake --build build                                   # builds the gen_* tools
B=build/test
$B/gen_vectors  data/JPLEPH | awk 'NR==1||NR%100==0' > test/vectors/golden_state_de440.csv
$B/gen_place    data/JPLEPH | awk 'NR==1||NR%50==0'  > test/vectors/golden_place_de440.csv
$B/gen_star     data/JPLEPH | awk 'NR==1||NR%10==0'  > test/vectors/golden_star_de440.csv
$B/gen_nutation             | awk 'NR==1||NR%10==0'  > test/vectors/golden_nutation.csv
$B/gen_hor                  | awk 'NR==1||NR%20==0'  > test/vectors/golden_hor.csv
```

## Prerequisites

`gen_vectors` needs the legacy NOVAS-C archive; build it once in the parent repo
(this fetches + patches + builds it):

```sh
cd ../..          # repo root
make              # produces novasc3.1/novas.a
```

CMake enables `gen_vectors` when `../../novasc3.1/novas.a` exists and wires the
`gen_state` fixture when a fetched ephemeris (`data/JPLEPH`) is also present.
`gen_constants` only needs `con440.c` (repo root); no NOVAS build. When a
prerequisite is missing the consuming test skips (exit 0) instead of failing, so
CI without the downloads stays green.

## Manual generation (for inspection)

```sh
cmake --build build --target gen_vectors gen_place gen_constants
./build/test/gen_vectors data/JPLEPH out_state.csv     # or stdout if omitted
./build/test/gen_place   data/JPLEPH out_place.csv     # or stdout if omitted
./build/test/gen_constants out_constants.csv           # or stdout if omitted
```

State-vector grid: every `state()` code path — all target/center reconstruction
branches (SSB, EMB, Earth, Moon, both Earth↔Moon directions, body-to-body with
Earth/Moon as center), both unit systems (AU/day and km/s via the `KM` flag),
two-part Julian dates (`jd_lo != 0`, incl. the recommended midnight split), and
boundary epochs (span ends, record boundary, sub-interval boundaries).

CSV columns:
- state: `units,target,center,jd_hi,jd_lo,px,py,pz,vx,vy,vz`
  (`units`: 0 = AU/day, 1 = km/s; `jd` split as `jd_hi + jd_lo`, TDB)
- place: `coord_sys,accuracy,body,where,lat,lon,height,delta_t,jd_tt,ra,dec,dis,rx,ry,rz,rv`
  (`coord_sys`: 0 = GCRS, 1 = true equator & equinox of date (apparent),
  2 = equator & CIO of date, 3 = astrometric; `accuracy`: 0 = full,
  1 = reduced; `body`: NOVAS number 1..11; `where`: 0 = geocenter, 1 = surface,
  with lat/lon in degrees, height in m, `delta_t` in s; ra in hours, dec in
  degrees, dis in AU, rv in km/s)
- star: `coord_sys,accuracy,star,where,lat,lon,height,delta_t,jd_tt,ra,dec,dis,rx,ry,rz,rv`
  (`star`: index into the shared catalog table mirrored in gen_star.c /
  test_star.cpp; otherwise as `place`, with dis = 0 for a star)
- nutation: `accuracy,jd_tdb,dpsi,deps,mean_obliq` (arcseconds)
- hor: `accuracy,ref,jd_ut1,delta_t,lat,lon,height,temp,pressure,xp,yp,ra,dec,zd,az,rar,decr`
  (`ref`: 0 none, 1 standard, 2 from location; angles in degrees, ra/rar in hours)
- time: `year,month,day,hour,jd,cy,cm,cd,chour`
  (`jd = julian_date(...)`; `c*` = `cal_date(jd)`)
- constants: `name,value`

## Nutation coefficient tables

The IAU 2000A / NU2000K series tables are extracted once from
`novasc3.1/nutation.c` into `src/nutation_tables.inc` (committed library source)
by `tools/extract_nutation_tables.py`. Regenerate only if the model changes:

```sh
tools/extract_nutation_tables.py ../novasc3.1/nutation.c src/nutation_tables.inc
```

`gen_constants` reflects `con440.c`, which is a DE440 transcription; the
`constants` test therefore skips unless the selected ephemeris is DE440.

## Notes

- If linking fails on `ephem_open`/`planet_ephemeris`, the symbols live in
  `eph_manager.o`; ensure it is inside `novas.a` (the parent Makefile's
  `support/Makefile.Cdist` controls the archive contents) or add the object to
  the link line in `test/CMakeLists.txt`.
- Additional oracles to fold in later (research doc 3.5): the USNO
  `checkout-*-usno.txt` files shipped in `novasc3.1/` (small enough to commit
  under `test/vectors/`), and end-to-end output of the legacy `planets` /
  `tropical` programs.
