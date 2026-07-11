# DE440 Ephemeris & NOVAS-C — Research and Design Foundation

Research notes toward replacing NOVAS-C with a modern C++23 library that consumes
the JPL DE440 binary ephemeris directly. This document is the anchor for that
effort: Part 1 describes what is actually stored in the ephemeris, Part 2 surveys
the NOVAS-C 3.1 API we are replacing, and Part 3 records the design direction and
decisions taken so far.

All concrete numbers in Part 1 were verified by parsing `linux_p1550p2650.440`
directly, not inferred from documentation.

---

## Part 1 — The DE440 ephemeris data model

### 1.1 File as a whole

The "Linux" binary (`linux_p1550p2650.440`, 102,272,352 bytes) is a flat array of
fixed-size **records of 8144 bytes = 1018 little-endian `double`s**.

- **12,558 records total** (1 header + 12,557 data records)
- **Coverage:** JED 2287184.5 → 2688976.5 (**1550-Dec-21 → 2650-Jan-25**)
- **32 days per record** (`SS[2]`)
- **Time scale:** TDB (a.k.a. T_eph), *not* UTC/TT
- **Reference frame:** ICRF / J2000 (Earth mean equator & equinox of J2000)

JPL's format specifies that record 1 (header) is followed by **record 2 of
constant values**, with data beginning at **physical record 3**. This is why
`eph_manager.c:state()` computes `nr = ((jd - SS[0]) / SS[2]) + 3`. NOVAS's
`ephem_open` parses only record 1; it never reads the constant values in record 2.

### 1.2 Header (record 1) — verified contents

Parse order follows `eph_manager.c:177-223`. Values are DE440's:

| Field | Type/size | DE440 value |
|---|---|---|
| `TTL` | 3×84 chars | "JPL Planetary Ephemeris DE440/LE440" + start/final epoch lines |
| `CNAM` | 400×6 chars | first 400 constant *names* |
| `SS[3]` | 3 doubles | `{2287184.5, 2688976.5, 32.0}` (begin, end, days/record) |
| `NCON` | int | **645** constants |
| `AU` | double | 149597870.7 km |
| `EMRAT` | double | 81.30056822149722 (Earth/Moon mass ratio) |
| `IPT[12][3]` | 36 ints | per-group record layout (see 1.3) |
| `DENUM` | int | 440 |
| `LPT[3]` | 3 ints | libration group layout |

**Subtlety — the constant-name overflow:** `NCON = 645` but `CNAM` in record 1
holds only **400** names. When `NCON > 400`, JPL stores the remaining 245 names in
record 2 (the `CNAM2` extension). NOVAS ignores this entirely. The repo's
`con440.c` is a hand-captured full dump of all 645 constant names **and values**
for DE440 — it exists precisely because NOVAS discards them. It is a golden
reference for validating a proper record-2 reader.

### 1.3 Per-group record layout (`IPT` / `LPT`)

Each data record holds Chebyshev **position** coefficients for 13 groups. Each
`IPT` triple is `(offset_1based, ncoeff_per_component, n_subintervals)`. Verified
for DE440:

| # | Group | Components | offset | ncoeff | subints | update cadence |
|---|---|---|---|---|---|---|
| 1 | Mercury | 3 | 3 | 14 | 4 | 8 d |
| 2 | Venus | 3 | 171 | 10 | 2 | 16 d |
| 3 | Earth-Moon barycenter | 3 | 231 | 13 | 2 | 16 d |
| 4 | Mars | 3 | 309 | 11 | 1 | 32 d |
| 5 | Jupiter | 3 | 342 | 8 | 1 | 32 d |
| 6 | Saturn | 3 | 366 | 7 | 1 | 32 d |
| 7 | Uranus | 3 | 387 | 6 | 1 | 32 d |
| 8 | Neptune | 3 | 405 | 6 | 1 | 32 d |
| 9 | Pluto | 3 | 423 | 6 | 1 | 32 d |
| 10 | Moon (geocentric) | 3 | 441 | 13 | 8 | 4 d |
| 11 | Sun (SSB-relative) | 3 | 753 | 11 | 2 | 16 d |
| 12 | Nutations Δψ, Δε | **2** | 819 | 10 | 4 | 8 d |
| 13 | Librations (φ, θ, ψ) | 3 | 899 | 10 | 4 | 8 d |

Two groups break the "3 components" assumption: **nutations have 2** (Δψ, Δε) and
**librations have 3** (lunar mantle Euler angles). Layout closes exactly:
`899 + 3·10·4 = 1019` → indices 1..1018 → 1018 doubles = 8144 bytes. ✔

Within a group the coefficient set is naturally a **rank-3 tensor**:
`[subinterval][component][coefficient]`. This is the primary motivation for using
`std::mdspan` (see Part 3).

### 1.4 The interpolation core (`eph_manager.c:interpolate`)

Position is a Chebyshev series of the first kind; velocity is its analytic
derivative (recurrence `VC[i] = 2·τ·VC[i-1] + 2·PC[i-1] − VC[i-2]`, scaled by
`2·na / interval`). Steps:

1. map TDB → record number and normalized time within the 32-day span,
2. select subinterval `l`, compute Chebyshev time `τ ∈ [-1, 1]`,
3. evaluate the `PC` basis via forward recurrence and dot against coefficients.

**Porting hazard:** the routine caches `PC[1]` across calls to skip re-evaluating
the basis when `τ` is unchanged. This stateful micro-optimization is a purity /
thread-safety hazard and should be replaced with a stateless evaluation (or
per-call local state) in the port.

### 1.5 Raw-layer semantics (what `state`/`planet_ephemeris` return)

- `state()` returns **SSB-relative** position/velocity for all groups **except the
  Moon**, which is **geocentric**.
- **Earth is not stored** — only the Earth-Moon barycenter (EMB) is.
- `planet_ephemeris()` reconstructs Earth and Moon barycentric states from EMB, the
  geocentric Moon, and `EMRAT`:
  `Earth = EMB − Moon / (1 + EMRAT)`, `Moon_bary = Earth + Moon`.
- Units: AU & AU/day by default (`KM=0`), or km & km/s (`KM=1`).

This EMB↔Earth↔Moon algebra is the single most error-prone thing to port and must
be property-tested against NOVAS.

### 1.6 The constants block (`con440.c`)

645 values, categories:

- integration metadata (`DENUM`, `JDEPOC = 2440400.5`);
- physical constants (`CLIGHT`, `AU`, `EMRAT`, `GM1..GM9`, `GMS`);
- initial-condition state vectors;
- Earth/Moon/Sun gravity harmonics & tides (`J2E`, `RE`, `K2M`, `TAUM`, `C22M`, …);
- lunar libration parameters;
- **~570 asteroid masses (`MA####`)**.

Most consumers need only `AU`, `EMRAT`, `CLIGHT`, `GM*`, `RE`. The modern library
should read these from **record 2 of the file** (a name→value map) rather than
hardcode them; `con440.c` is the golden reference to validate that reader.

### 1.7 DE440 vs DE441

DE440 covers 1550–2650 (this repo). DE441 (`lnxm13000p17000.431`-style naming for
the long-span file) covers ~13000 BC–17000 AD. Same record structure and reader;
only `SS`, record count, and (marginally) some layout differ. The library must not
assume DE440's specific span or `IPT` values — it should read them from the header.

---

## Part 2 — NOVAS-C 3.1 API survey

NOVAS-C is ~60 functions in `novas.h` over a three-layer stack:

```
Layer 0  eph_manager.c   raw JPL binary reader + Chebyshev interpolation
Layer 1  solsysN.c       "solarsystem" adapter (solsys1 wraps eph_manager)
Layer 2  novas.c         astrometric reductions (the actual library)
```

`solarsystem.h` is the seam: it declares `solarsystem()` / `solarsystem_hp()` and
`ephemeris()`. This project links `solsys1.c` (backed by `eph_manager`). `solsys2`
uses a Fortran JPL library; `solsys3` is an analytic Sun/Earth-only fallback.

### 2.1 Functional grouping (Layer 2)

- **Top-level "sky position" (the 80% API):** `place()` (the hub), and conveniences
  `app_planet` / `topo_planet` / `astro_planet` / `virtual_planet`,
  `app_star` / `topo_star` / `local_star` / `astro_star` / `virtual_star`,
  `mean_star`.
- **Object/observer construction:** `make_object`, `make_cat_entry`,
  `make_observer{,_at_geocenter,_on_surface,_in_space}`, `make_on_surface`,
  `make_in_space`.
- **Coordinate transforms:** `equ2hor`, `equ2ecl(_vec)`, `ecl2equ_vec`, `equ2gal`,
  `gcrs2equ`, `ter2cel` / `cel2ter`, `frame_tie`, `spin`, `wobble`, `precession`,
  `nutation` (+ `nutation_angles`), `e_tilt`, `mean_obliq`, `cel_pole`.
- **Time & rotation:** `sidereal_time`, `era`, `julian_date`, `cal_date`, `tdb2tt`.
- **CIO machinery:** `cio_ra`, `cio_location`, `cio_basis`, `cio_array`,
  `ira_equinox`.
- **Physics building blocks:** `light_time`, `d_light`, `aberration`, `grav_def`,
  `grav_vec`, `rad_vel`, `proper_motion`, `bary2obs`, `geo_posvel`, `terra`,
  `limb_angle`, `refract`.
- **Vector/utility:** `vector2radec`, `radec2vector`, `starvectors`, `norm_ang`,
  `transform_cat`, `transform_hip`, `ephemeris`.

### 2.2 The `place()` pipeline (conceptual heart)

`place(jd_tt, object, observer, delta_t, coord_sys, accuracy, → sky_pos)` composes:
ephemeris lookup → light-time iteration → gravitational deflection (`grav_def`) →
aberration → frame tie / precession-nutation into the requested `coord_sys`.

- `coord_sys ∈ {GCRS(0), equator&equinox-of-date(1), equator&CIO-of-date(2),
  astrometric(3)}` — aliased in `ephutil.h` as `coord_gcrs`…`coord_astro`.
- `accuracy ∈ {full(0), reduced(1)}`.

`place` + `equ2hor` is essentially the whole of what `planets.c` uses.

### 2.3 MVP surface (what the sample apps actually exercise)

From `planets.c` / `tropical.c`: `ephem_open/close`, `make_observer*`,
`make_object`, `make_cat_entry`, `place`, `equ2hor`, `sidereal_time`, `era`,
`cel2ter`, `radec2vector` / `vector2radec`, `julian_date`, plus the repo's own time
model (`make_time_parameters`, leap-second table, `delta_t`). That is roughly 10%
of NOVAS's function count.

### 2.4 Warts not to carry forward

- **Global mutable state:** `eph_manager` keeps the open file, record buffer, and
  interpolation cache (`PC`, `NRL`, `KM`, …) in file-scope globals → not
  thread-safe, not reentrant, one ephemeris at a time.
- **`short int` return codes** with per-function numbering; outputs via pointer args.
- **C-string-in-struct identity** (`cat_entry.starname[51]`, `catalog[4]`).
- **Split Julian date** as `double[2]` passed positionally; time scale (TT/TDB/UT1)
  is convention-only, not type-enforced — a rich source of silent bugs.
- **Raw `double*` vectors** with implicit units and frames.
- **`object.type/number` integer discriminated union** instead of a real variant.

---

## Part 3 — Modern C++23 design direction

### 3.1 Decisions taken

- **Deliverable of the research phase:** this document (persisted; reviewed before
  any code).
- **Coefficient tensor / mdspan gap:** **vendor the Kokkos reference `std::mdspan`**
  (single-header, standard-track API) now, since the toolchain lacks `<mdspan>`.
  Swapping to the standard header later is a no-cost migration.
- **Target scope:** the **full reduction layer** — aim for NOVAS parity across the
  `place()` pipeline and the coordinate/transform surface, not just the two-app MVP.
  (The MVP subset in 2.3 is still the natural first milestone toward that goal.)

### 3.2 Proposed layering (mirror NOVAS's seams, modernize each)

1. **`ephemeris` (Layer 0):** an opened DE file as an RAII value type;
   `state(target, TdbInstant) -> StateVector`. No globals; multiple instances
   allowed. Header + record-2 constants parsed into a `constants` map.
2. **`reductions` (Layer 2):** free functions over strong types — `place()`,
   `equ2hor()`, coordinate transforms.
3. **Vocabulary types:** distinct `TdbInstant` / `TtInstant` / `Ut1Instant`,
   `Angle`, `AuLength`, and frame-tagged 3-vectors.

### 3.3 Where `std::mdspan` fits

Each record group is a rank-3 view:
`mdspan<const double, extents<size_t, dynamic, components, ncoeff>>` over the
1018-double record buffer, indexed `[subinterval, component, coeff]`. This yields
zero-copy per-body coefficient views. Kokkos mdspan is vendored to provide it
today (decision in 3.1).

### 3.4 C++23 features to lean on

- `std::mdspan` (vendored) for the coefficient tensor and coefficient views.
- `std::expected<T, eph_error>` to replace `short int` return codes.
- Multidimensional `operator[]` for record/granule indexing.
- `std::span` for vector I/O; `enum class` / strong typedefs for bodies, coord
  systems, accuracy.
- `constexpr` / `consteval` for constants and unit conversions; `std::format` to
  replace the DMS/HMS string builders in `strutil.c`.
- `import std;` if the toolchain's module support is solid (verify in the flake).
- Keep the Clenshaw recurrence an explicit numeric loop (accuracy/perf over ranges).

### 3.5 Test-vector strategy

Three oracle sources, increasing in value:

1. **USNO checkout files** already in `novasc3.1/`: `checkout-mp-usno.txt`,
   `checkout-stars-usno.txt`, `example-usno.txt` — JPL/USNO-blessed expected outputs
   for `checkout-mp.c`, `checkout-stars.c`, `example.c`. Free regression vectors.
2. **Generated vectors from `novas.a`** (already built in-repo): a small C harness
   sweeping dates/bodies/observers through `state`, `planet_ephemeris`, `place`,
   `equ2hor`, dumping `(input → output)` to a data file the C++ tests load. This
   pins the port to bit-comparable NOVAS behavior across the whole DE440 span.
3. **The two apps as end-to-end oracles:** `planets` / `tropical` output (the README
   captures `tropical`'s) as golden acceptance tests.

Build the vector generator first — it makes the port test-driven from line one.
Suggested tolerances: `state`/interp match NOVAS to ~1e-13 AU (same algorithm);
reduction outputs to sub-milliarcsecond, well inside model noise.

### 3.6 Suggested build order

1. NOVAS test-vector generator against `novas.a` (unblocks TDD).
2. Layer 0 (`ephemeris` value type + mdspan record view); validate
   `state`/`planet_ephemeris` against generated vectors — the riskiest numeric ground.
3. Vocabulary types (time scales, angles, frame-tagged vectors).
4. Layer 2 `place()` pipeline and the coordinate/transform surface, toward full
   NOVAS parity.

---

## References

- `con440.c` — full DE440 constant name/value dump (645 entries).
- `novasc3.1/eph_manager.{c,h}` — raw binary reader + Chebyshev interpolation.
- `novasc3.1/novas.h` — full NOVAS-C 3.1 API.
- `novasc3.1/solarsystem.h`, `solsys1.c` — Layer-1 adapter seam.
- `ephutil/ephutil.{c,h}` — this project's time model, leap seconds, header reader.
- `planets/planets.c`, `tropical/tropical.c` — reference consumers.
- Park, R. S., et al. "The JPL Planetary and Lunar Ephemerides DE440 and DE441."
