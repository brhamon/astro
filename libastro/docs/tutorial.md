# libastro tutorial

A guided tour from "open the ephemeris" to "when does the Sun rise?". Each step
is a small, complete program. If you'd rather not write C++, every step has a
one-line [`astro` CLI](#doing-it-from-the-command-line) equivalent — skip to the
bottom.

For the full API, see [`reference.md`](reference.md).

## 0. Setup

You need a C++23 toolchain (GCC 15+ or recent Clang), CMake 3.28+, and the DE440
binary ephemeris.

```sh
./scripts/fetch_ephemeris.sh        # curls DE440 into data/JPLEPH (verifies sha256)
cmake -B build                      # -G Ninja optional
cmake --build build                 # produces build/libastro.a
```

Compile any program in this tutorial against the built library. `std::mdspan` is
a private implementation detail, so consumers don't need it:

```sh
g++ -std=c++23 -I include prog.cpp build/libastro.a -o prog && ./prog
```

All programs assume you run them from the `libastro/` directory so `data/JPLEPH`
resolves; otherwise pass a full path to `Ephemeris::open`.

## 1. Open the ephemeris and read a state vector

`Ephemeris::open` returns a `std::expected` — check it before use. `state()`
gives position and velocity of one point relative to another.

```cpp
#include <cstdio>
#include "astro/ephemeris.hpp"
#include "astro/time.hpp"
using namespace astro;

int main() {
  auto eph = Ephemeris::open("data/JPLEPH");
  if (!eph) {
    std::printf("open failed: %s\n", std::string(to_string(eph.error())).c_str());
    return 1;
  }

  // The ephemeris runs on TDB; convert a civil UTC date to it.
  const TdbInstant t = tdb_from_tt(utc_time_scales(2026, 7, 11, 0.0).tt);

  auto mars = eph->state(Point::mars, Point::sun, t);   // heliocentric Mars
  if (!mars) return 1;
  std::printf("Mars (heliocentric) = % .6f % .6f % .6f AU\n",
              mars->position[0], mars->position[1], mars->position[2]);
}
```

`Point::sun` as the center gives heliocentric coordinates;
`Point::solar_system_barycenter` gives barycentric; `Point::earth` gives
geocentric.

## 2. Where is a planet in the sky?

`place()` reduces an ephemeris state to an apparent RA/Dec (light-time,
deflection, aberration, precession/nutation). `equ2hor()` then turns that into
altitude and azimuth for an observer. `utc_time_scales` hands you every time
scale each step needs.

```cpp
#include <cstdio>
#include "astro/ephemeris.hpp"
#include "astro/reductions.hpp"
#include "astro/time.hpp"
using namespace astro;

int main() {
  auto eph = Ephemeris::open("data/JPLEPH");
  if (!eph) return 1;

  const auto ts = utc_time_scales(2026, 7, 11, 4.0);       // 2026-07-11 04:00 UTC
  const SurfaceObserver seattle{47.61, -122.33, 10.0};     // lat, lon(E+), height m

  auto sky = place(*eph, Point::jupiter, ts.tt, ts.delta_t, seattle,
                   CoordSys::equator_equinox, Accuracy::full);
  if (!sky) return 1;

  const auto hor = equ2hor(ts.ut1, ts.delta_t, Accuracy::full, /*polar motion*/ {},
                           seattle, sky->ra_hours, sky->dec_deg,
                           Refraction::from_location);

  std::printf("Jupiter  RA %.4f h  Dec %+.4f deg   Alt %+.2f deg  Az %.2f deg\n",
              sky->ra_hours, sky->dec_deg,
              90.0 - hor.zenith_distance_deg, hor.azimuth_deg);
}
```

Drop the `seattle` argument from `place()` for a geocentric position. Swap
`CoordSys::equator_equinox` for `gcrs`, `astrometric`, or `equator_cio` to change
the output frame.

## 3. Lazy event streams

The phenomena layer answers "when does X happen?" as a stream you pull from —
forward or backward from a start time — instead of a fixed table. A helper to
show TT instants as civil UTC (accurate to < 1 s):

```cpp
#include "astro/time.hpp"
using namespace astro;

CivilDate tt_to_utc(double tt_jd) {                 // approximate: ignores UT1-UTC
  const double leap = tai_minus_utc(tt_jd);
  return calendar_date(tt_jd - (leap + 32.184) / 86400.0);
}
```

### When does the Sun rise and set?

```cpp
#include <cstdio>
#include <ranges>
#include "astro/phenomena.hpp"
// ... eph, ts, seattle, tt_to_utc as above ...

for (auto e : horizon_events(*eph, Point::sun, seattle, ts.tt,
                             Horizon::sun_upper_limb, Direction::forward,
                             ts.delta_t) | std::views::take(4)) {
  const char* k = e.kind == EventKind::rise ? "rise"
                : e.kind == EventKind::set  ? "set"  : "transit";
  const auto c = tt_to_utc(e.time.jd.value());
  std::printf("%-8s %04d-%02d-%02d %06.3f UTC  alt=%+.3f az=%.3f\n",
              k, c.year, c.month, c.day, c.hour, e.altitude_deg, e.azimuth_deg);
}
```

`std::views::take(4)` is the only thing bounding the (conceptually infinite)
stream — exactly four events are computed. `Horizon` picks the convention
(`sun_upper_limb` for sunrise/sunset, `civil_twilight` for dawn/dusk, `star` for
a point source, and so on). Pass `Direction::backward` to walk into the past.

### When is the next equinox or solstice?

```cpp
#include "astro/phenomena.hpp"

for (auto m : tropical_moments(*eph, ts.tt) | std::views::take(1)) {
  static const char* names[] = {"March equinox", "June solstice",
                                "September equinox", "December solstice"};
  const auto c = tt_to_utc(m.time.jd.value());
  std::printf("Next: %-18s %04d-%02d-%02d %06.3f UTC\n",
              names[static_cast<int>(m.season)], c.year, c.month, c.day, c.hour);
}
```

### When is Earth closest to the Sun?

Apsides come straight from the state velocity (the point where r·v = 0), so
they're exact and need no observer.

```cpp
#include "astro/phenomena.hpp"

for (auto a : apsides(*eph, Point::earth, Point::sun, ts.tt) | std::views::take(2)) {
  const auto c = tt_to_utc(a.time.jd.value());
  std::printf("%-10s %04d-%02d-%02d  %.6f AU\n",
              a.kind == Apsis::periapsis ? "perihelion" : "aphelion",
              c.year, c.month, c.day, a.distance_au);
}
```

Use `Point::moon` about `Point::earth` for the Moon's perigee/apogee.

## Doing it from the command line

The `astro` tool (built by default) covers the same ground without writing code:

```sh
export LIBASTRO_EPHEMERIS=$PWD/data/JPLEPH
build/cli/astro state   mars 2026-07-11 --center sun
build/cli/astro place   jupiter 2026-07-11T04:00 --observer 47.61,-122.33,10
build/cli/astro sky     2026-07-11T04:00 --observer 47.61,-122.33,10
build/cli/astro rise    sun 2026-07-11 --observer 47.61,-122.33,10 --horizon sun -n 4
build/cli/astro seasons 2026-01-01 -n 4
build/cli/astro apsides earth 2026-01-01 -n 4
```

See the [CLI table in the reference](reference.md#the-astro-cli) for every
subcommand, and `astro <command> --help` for its options.

## Where to go next

- [`reference.md`](reference.md) — every type and function, with the NOVAS-C
  analogue.
- [`../../docs/de440-and-novas-research.md`](../../docs/de440-and-novas-research.md)
  — the DE440 data model and the design rationale.
- A couple of habits worth keeping:
  - **Always check the `std::expected`.** `open` and `place` can fail (missing
    file, epoch out of range); `result.error()` says why.
  - **Mind the time scale.** The types stop you passing TT where UT1 is wanted;
    let the compiler help. `utc_time_scales` produces all of them at once.
