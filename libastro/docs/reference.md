# libastro API reference

A layer-by-layer tour of the public surface. Everything lives in namespace
`astro`. Headers are under `include/astro/`; include what you use (or the
umbrella per layer noted below).

- **Vocabulary** — strongly-typed times, bodies, states, errors.
- **Time** (`astro/time.hpp`) — civil calendar, leap seconds, scale conversions.
- **Layer 0** (`astro/ephemeris.hpp`) — the ephemeris file and raw state vectors.
- **Layer 2** (`astro/reductions.hpp`) — `place()`, `equ2hor`, sidereal time.
- **Layer 3** (`astro/phenomena.hpp`) — lazy event streams (seasons, rise/set,
  apsides).
- **CLI** — the `astro` command-line tool.

For *why* the library is shaped this way, and the NOVAS-C mapping in full, see
[`../../docs/de440-and-novas-research.md`](../../docs/de440-and-novas-research.md).

---

## Conventions

- **Errors.** Fallible calls return `std::expected<T, EphError>`; there are no
  exceptions and no error-code out-params. Check with `if (result)` and read
  `result.error()` on failure (`to_string(EphError)` gives a message).
- **Angles.** Right ascension is in **hours**, declination / longitude /
  latitude / azimuth / zenith distance in **degrees**, unless a name says
  otherwise.
- **Distances / velocities.** AU and AU/day by default (`Units::au`), or km and
  km/s (`Units::km`) where selectable.
- **Time scales are types, not comments.** A `TtInstant` cannot be silently
  passed where UT1 is meant — the compiler enforces it.

### Vocabulary types

| Type | Header | Meaning |
|------|--------|---------|
| `JulianDate{whole, frac}` | `time_scales.hpp` | Two-part JD; `.value()` = `whole + frac`. Split defers cancellation. |
| `TtInstant`, `TdbInstant`, `Ut1Instant` | `time_scales.hpp` | Instants tagged by time scale; each holds a `JulianDate jd`. |
| `DeltaT{seconds}` | `time_scales.hpp` | ΔT = TT − UT1, seconds. |
| `Point` | `body.hpp` | Solar-system point (`mercury`…`pluto`, `moon`, `sun`, `earth`, `solar_system_barycenter`, `earth_moon_barycenter`). `to_string(Point)` for a label. |
| `Units` | `state_vector.hpp` | `au` (AU, AU/day) or `km` (km, km/s). |
| `Vec3` | `state_vector.hpp` | `std::array<double,3>`. |
| `StateVector{position, velocity, units}` | `state_vector.hpp` | ICRF/J2000 position + velocity. |
| `Accuracy` | `accuracy.hpp` | `full` (IAU 2000A, 3-body deflection) or `reduced` (NU2000K, Sun-only). |
| `EphError` | `error.hpp` | Error enum; `to_string(EphError)` → message. |

---

## Time (`astro/time.hpp`)

```cpp
double     julian_date(int year, int month, int day, double hour = 0.0);
CivilDate  calendar_date(double jd);            // {year, month, day, hour}
double     tai_minus_utc(double jd_utc);        // cumulative leap seconds
TimeScaleSet utc_time_scales(double jd_utc, double ut1_utc = 0.0);
TimeScaleSet utc_time_scales(int y, int mo, int d, double hour, double ut1_utc = 0.0);
double     tdb_minus_tt_seconds(double jd_tt);  // periodic term, |·| < ~2 ms
TdbInstant tdb_from_tt(TtInstant t);
```

`utc_time_scales` is the usual entry point: give it a civil UTC date (and
optionally UT1−UTC from IERS Bulletin A/B; 0 is good to < ~0.9 s) and it returns

```cpp
struct TimeScaleSet {
  double jd_utc;
  double leap_seconds;   // TAI − UTC
  TtInstant  tt;         // = UTC + (TAI−UTC) + 32.184 s
  Ut1Instant ut1;        // = UTC + (UT1−UTC)
  DeltaT     delta_t;    // TT − UT1
};
```

which feeds straight into `place()` / `equ2hor` / the phenomena streams. The
calendar routines port NOVAS `julian_date` / `cal_date`; the leap-second table
lives in `src/time.cpp` (current through 2017-01-01 = 37 s).

---

## Layer 0 — the ephemeris (`astro/ephemeris.hpp`)

```cpp
class Ephemeris {                          // movable, non-copyable (owns a file)
  static std::expected<Ephemeris, EphError> open(const std::filesystem::path&);
  const Header&    header()    const noexcept;
  const Constants& constants() const noexcept;
  bool             covers(TdbInstant t) const noexcept;
  std::expected<StateVector, EphError>
      state(Point target, Point center, TdbInstant t, Units = Units::au) const;
};
```

- **`open`** parses record 1 (the header) and record 2 (the constant block).
  Failure modes mirror NOVAS `ephem_open` (`file_not_found`, `bad_header`,
  `unsupported_denum`).
- **`state`** reproduces `planet_ephemeris`: Chebyshev interpolation with the
  full EMB/Moon/Earth reconstruction via EMRAT. `target == center` is the zero
  state. Out-of-range epochs give `epoch_out_of_range`.
- **`header()`** → `Header{title, jd_begin, jd_end, days_per_record, denum,
  n_constants, au_km, earth_moon_ratio, record_length, record_count, groups}`.
- **`constants()`** → `Constants`; `constants().get("AU")` returns
  `std::optional<double>` for a named record-2 constant.

Unlike NOVAS's file-scope `eph_manager`, each `Ephemeris` is an independent RAII
value — you may open several at once, and they are not global.

---

## Layer 2 — reductions (`astro/reductions.hpp`)

### `place` — ephemeris state → on-sky position

```cpp
std::expected<SkyPos, EphError> place(const Ephemeris&, Point body,
    TtInstant t, DeltaT dt, CoordSys sys, Accuracy acc);                    // geocentric
std::expected<SkyPos, EphError> place(const Ephemeris&, Point body,
    TtInstant t, DeltaT dt, const SurfaceObserver&, CoordSys, Accuracy);    // topocentric
std::expected<SkyPos, EphError> place(const Ephemeris&, const Star&,
    TtInstant t, DeltaT dt, CoordSys, Accuracy);                            // star, geocentric
std::expected<SkyPos, EphError> place(const Ephemeris&, const Star&,
    TtInstant t, DeltaT dt, const SurfaceObserver&, CoordSys, Accuracy);    // star, topocentric
```

The hub (NOVAS `place`): light-time, gravitational deflection, aberration, and
the chosen frame. Output:

```cpp
struct SkyPos {
  Vec3   r_hat;                  // unit vector toward the object
  double ra_hours, dec_deg;
  double distance_au;            // 0 for a star (see parallax_distance_au)
  double radial_velocity_km_s;
};
```

- **`CoordSys`** — `gcrs` (deflection + aberration only), `astrometric`
  (neither), `equator_equinox` (apparent place of date), `equator_cio` (equator
  & CIO of date, analytic — no CIO data file needed).
- **`Accuracy`** — `full` or `reduced` (see the vocabulary table).
- **`SurfaceObserver{latitude_deg, longitude_deg, height_m, temperature_c=10,
  pressure_mbar=1010}`** — geodetic (ITRS), north/east positive.
- **`Star{ra_hours, dec_deg, pm_ra_mas_yr, pm_dec_mas_yr, parallax_mas,
  radial_velocity_km_s}`** — ICRS catalog data. `parallax_distance_au(star)`
  gives 1/sin(parallax) in AU for display (a star's distance isn't part of the
  astrometric place).

### `equ2hor` — apparent RA/Dec → horizon

```cpp
HorizonPos equ2hor(Ut1Instant t, DeltaT dt, Accuracy, PolarMotion,
                   const SurfaceObserver&, double ra_hours, double dec_deg,
                   Refraction);
```

Applies polar motion, Earth rotation, and optional refraction.

```cpp
struct HorizonPos { double zenith_distance_deg, azimuth_deg,   // 0 = zenith; az E of N
                           ra_refracted_hours, dec_refracted_deg; };
enum class Refraction { none, standard, from_location };       // from_location: use observer T/P
struct PolarMotion { double x_arcsec = 0, y_arcsec = 0; };     // 0,0 to ignore
```

Altitude is `90 − zenith_distance_deg`.

### `greenwich_apparent_sidereal_time`

```cpp
double greenwich_apparent_sidereal_time(Ut1Instant t, DeltaT dt, Accuracy);  // hours [0,24)
```

GAST = GMST + equation of the equinoxes (NOVAS `sidereal_time`, apparent). Local
apparent sidereal time is `GAST + longitude/15`.

---

## Layer 3 — phenomena (`astro/phenomena.hpp`)

Derived events as **lazy `std::generator` streams** rather than tables. Each
runs forward or backward from a start time and computes only what you pull, so
`std::views::take(n)` (or breaking the loop) is the natural limit.

```cpp
enum class Direction { forward, backward };
```

### Tropical moments — equinoxes and solstices

```cpp
std::generator<SeasonalMoment>
    tropical_moments(const Ephemeris&, TtInstant start, Direction = forward);

enum class Season { march_equinox, june_solstice, september_equinox, december_solstice };
struct SeasonalMoment { Season season; TtInstant time; };
```

Defined by the Sun's apparent ecliptic longitude reaching a multiple of 90°.

### Rise / transit / set

```cpp
std::generator<SkyEvent>
    horizon_events(const Ephemeris&, Point body, const SurfaceObserver&,
                   TtInstant start, Horizon = Horizon::star,
                   Direction = forward, DeltaT = {});

enum class EventKind { rise, upper_transit, set, lower_transit };
enum class Horizon { geometric, star, sun_upper_limb, moon,
                     civil_twilight, nautical_twilight, astronomical_twilight };
struct SkyEvent { EventKind kind; TtInstant time; double altitude_deg, azimuth_deg; };
```

`Horizon` selects the standard altitude h₀ (geometric 0°, star −0.5667°, sun
upper limb −0.8333°, moon +0.125°, twilights −6/−12/−18°). Rise/set are
altitude = h₀ crossings; transits are hour-angle zero crossings. Circumpolar or
never-rising bodies simply yield no rise/set (transits still occur).

### Apsides — perihelion/aphelion, perigee/apogee

```cpp
std::generator<ApsisEvent>
    apsides(const Ephemeris&, Point body, Point center, TtInstant start,
            Direction = forward);

enum class Apsis { periapsis, apoapsis };
struct ApsisEvent { Apsis kind; TtInstant time; double distance_au; };
```

Distance extrema of `body` about `center` (where the radial velocity r·v = 0,
read exactly from the state velocity). `center = Point::sun` gives
perihelion/aphelion for a planet (or Earth); `center = Point::earth` gives the
Moon's perigee/apogee.

### Helpers

```cpp
void   equ_to_ecl_of_date(double jd_tt, double ra_h, double dec_deg,
                          double& ecl_lon_deg, double& ecl_lat_deg);  // vs NOVAS equ2ecl
double sun_apparent_longitude(const Ephemeris&, TtInstant t);         // [0,360) deg
```

---

## The `astro` CLI

One tool over the whole surface (built with `-DLIBASTRO_BUILD_CLI=ON`, default
for a top-level build). Times are civil UTC (`YYYY-MM-DD[THH:MM[:SS]]`), or the
literal `now` for the current system time in UTC; the ephemeris is located via
`--ephemeris`, `$LIBASTRO_EPHEMERIS`, or `./data/JPLEPH`.

| Command | Purpose |
|---------|---------|
| `astro info` | ephemeris header |
| `astro time <utc>` | UTC → TT/UT1/TDB + leap seconds |
| `astro constant <NAME>…` | named record-2 constants |
| `astro state <body> <utc> [--center ssb\|sun\|earth\|emb]` | Layer 0 state vector |
| `astro place <body> <utc> [--observer lat,lon,h] [--coord apparent\|gcrs\|astrometric\|cio] [--accuracy full\|reduced]` | apparent place |
| `astro sky <utc> [--observer lat,lon,h]` | planet + Polaris table (RA/Dec/dist, alt/az) |
| `astro rise <body> <utc> --observer lat,lon,h [--horizon …] [-n N] [--back]` | rise/transit/set stream |
| `astro seasons <utc> [-n N] [--back]` | equinox/solstice stream |
| `astro apsides <body> <utc> [--center sun\|earth] [-n N] [--back]` | perihelion/aphelion or perigee/apogee |

Run `astro <command> --help` for the options of any subcommand.
