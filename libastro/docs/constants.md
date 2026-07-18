# Ephemeris constants reference

Every JPL DE ephemeris carries a block of named scalar constants — the values
the integration used and the physical/model parameters behind it. They live in
**record 2** of the binary file; their names come from `CNAM` (and, past 400,
the `CNAM2` overflow) in the header. `Ephemeris::open()` parses every
name/value pair into `Constants`, and `test/unit/test_constants.cpp` checks each
one bit-for-bit against the `con440.c` transcription.

**DE440 carries 645 constants.** The exact set is ephemeris-specific — a
different DE (441, or a future release) may add, drop, or revalue entries — so
treat this document as a guide to DE440 and always query the file itself for
ground truth.

## Querying them

```sh
astro constant                 # list every constant (name = value), sorted
astro constant AU EMRAT GMS    # look up specific ones
```

In code:

```cpp
auto eph = astro::Ephemeris::open("data/JPLEPH");
if (auto au = eph->constants().get("AU")) /* *au == 149597870.7 */;
for (const auto& [name, value] : eph->constants().entries()) { /* ... */ }
```

`get()` returns `std::optional<double>` (empty if the name is absent);
`entries()` is the whole map (unspecified order — sort by name if you need a
stable listing). There are also typed shortcuts: `au_km()`,
`earth_moon_ratio()`, `speed_of_light_km_s()`, `earth_radius_km()`.

## Units

| Quantity | Unit |
|----------|------|
| GM (mass parameter) | AU³/day² |
| Length (AU, radii) | km |
| Speed of light | km/s |
| Epoch / dates | Julian date (TDB) |
| Zonal/tesseral harmonics (J, C, S) | dimensionless |
| Angles (libration Euler angles) | radians |
| Ratios (EMRAT, LBET, LGAM) | dimensionless |

## Fundamental / identity

| Name | Meaning | DE440 value |
|------|---------|-------------|
| `DENUM` | DE (planetary) ephemeris number | 440 |
| `LENUM` | LE (lunar) ephemeris number | 440 |
| `CENTER` | integration center (0 = solar-system barycenter) | 0 |
| `JDEPOC` | epoch of the initial conditions (JD, TDB) | 2440400.5 |
| `TDATEB`, `TDATEF` | integration begin / final date stamps | — |
| `AU` | astronomical unit, in km | 149597870.7 |
| `CLIGHT` | speed of light, in km/s | 299792.458 |
| `EMRAT` | Earth / Moon mass ratio | 81.30056822 |

## Mass parameters (GM)

`GM<n>` is the gravitational parameter of a body, in AU³/day². The digit follows
the DE body numbering; note there is **no `GM3`** — the Earth is not integrated
separately, so the Earth–Moon system is `GMB` split by `EMRAT`
(GM⊕ = GMB·EMRAT/(EMRAT+1), GM☾ = GMB/(EMRAT+1)).

| Name | Body |
|------|------|
| `GMS` | Sun |
| `GM1` … `GM2` | Mercury, Venus |
| `GM4` … `GM9` | Mars, Jupiter, Saturn, Uranus, Neptune, Pluto |
| `GMB` | Earth–Moon barycenter |
| `GMSDOT` | secular rate of the Sun's GM (AU³/day² per day) |

These are exactly the values libastro uses to size the apsides march stride
(`GMS`, and `GMB`/`EMRAT` for the Moon).

**Asteroid masses.** `MA0001` … (through `MA8236`, 409 entries) are the GMs of
the perturbing asteroids in the DE440 asteroid model, in AU³/day². The numeric
tag is JPL's index into that model; the asteroid identities are documented by
JPL (see the DE440/441 paper below), not encoded in the ephemeris.

## Body sizes and gravity fields

| Name | Meaning |
|------|---------|
| `RE` | Earth equatorial radius (km) |
| `ASUN` | Sun radius (km) |
| `AM` | Moon radius (km) |
| `J2E`, `J3E`, `J4E` | Earth zonal harmonics |
| `J2EDOT`, `J2ET2` | secular / quadratic time variation of Earth `J2` |
| `J2SUN`, `J3SUN`, `J4SUN` | Sun zonal harmonics |
| `J2M` … `J9M` | Moon zonal harmonics (degree 2–9) |
| `C21M` … `C99M`, `S21M` … | Moon tesseral/sectoral harmonics (cosine/sine, to degree 9) |
| `MOISUN` | Sun moment-of-inertia factor |

## Tides and Love numbers

| Name | Meaning |
|------|---------|
| `K2E0`, `K2E1`, `K2E2` | Earth potential Love number *k₂* per tidal band (long-period, diurnal, semidiurnal) |
| `TAUE0`, `TAUE1`, `TAUE2` | Earth tidal time lags for those bands |
| `TAUER1`, `TAUER2` | Earth radial-tide time lags |
| `K2M` | Moon Love number *k₂* |
| `TAUM` | Moon tidal time lag |

## Lunar rotation, libration, and fluid core

The Moon's orientation is integrated alongside its orbit; these are the rotation
model's parameters and initial conditions.

| Name | Meaning |
|------|---------|
| `PHI`, `THT`, `PSI` | lunar (mantle) libration Euler angles at `JDEPOC` (rad) |
| `PHIC`, `THTC`, `PSIC` | lunar fluid-core Euler angles at `JDEPOC` (rad) |
| `PSIDOT` | rate of `PSI` |
| `OMEGAX/Y/Z` | mantle angular-velocity components at `JDEPOC` |
| `OMGCX/Y/Z` | fluid-core angular-velocity components at `JDEPOC` |
| `LBET` (β), `LGAM` (γ) | lunar moment-of-inertia ratios |
| `K2M`, `TAUM` | (see tides above) |
| `COBLAT`, `KVC` | lunar fluid-core flattening / core–mantle coupling parameters |

Their precise definitions are given in JPL's DE440 documentation; the group as a
whole drives the lunar physical-libration and core-friction model.

## Sun and Earth orientation

| Name | Meaning |
|------|---------|
| `OMGSUN` | solar rotation rate |
| `ROTEX`, `ROTEY` | Earth mantle orientation offsets |
| `DROTEX`, `DROTEY` | rates of `ROTEX`, `ROTEY` |
| `IFAC` | Earth-figure interaction scale factor |

## Relativity (PPN)

| Name | Meaning | DE440 value |
|------|---------|-------------|
| `BETA` | PPN parameter β (1 in general relativity) | 1 |
| `GAMMA` | PPN parameter γ (1 in general relativity) | 1 |

## Initial conditions at the epoch

`X<b>`, `Y<b>`, `Z<b>` and `XD<b>`, `YD<b>`, `ZD<b>` are the position (AU) and
velocity (AU/day) components of each integrated body at `JDEPOC`, where the
suffix `<b>` identifies the body: the digits `1,2,4…9` for the planets, `B` for
the Earth–Moon barycenter, `M` for the Moon, `S` for the Sun (and `C` for the
remaining integrated element). These are the seed state of the numerical
integration, not something you'd normally consume — for a state at an arbitrary
epoch use `Ephemeris::state()`.

## Authoritative source

For the exhaustive definitions — including every asteroid's identity and the
exact form of the lunar/relativity model parameters — see JPL's DE440/DE441
release: **Park, Folkner, Williams & Boggs, "The JPL Planetary and Lunar
Ephemerides DE440 and DE441," *AJ* 161:105 (2021)**, and the ASCII header
(`header.440`) distributed with the ephemeris.
