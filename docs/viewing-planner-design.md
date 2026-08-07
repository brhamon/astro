# astro viewing planner — architecture & roadmap

> **Status:** design / roadmap. No code exists yet for this; it records the
> direction so any single increment can be picked up independently, "as time
> allows." It complements
> [`de440-and-novas-research.md`](de440-and-novas-research.md) (the DE440 /
> NOVAS-C data model) and the libastro
> [`reference`](../libastro/docs/reference.md) / [`tutorial`](../libastro/docs/tutorial.md).

## 1. Vision & the guiding question

Make `astro` genuinely useful to amateur and enthusiast sky-watchers planning
near-future observing. The entry question is deliberately simple:

> **For the next few days (up to ~7), when are my best viewing opportunities,
> from a visibility perspective?**

"Best" is a funnel of constraints, evaluated roughly in this order:

1. **Where** — optimize the observing location to escape light pollution
   (static, slow-changing).
2. **When** — find the **dark**, **moonless**, **clear** windows at that
   location over the next N days.
3. **What** — match observable **phenomena** to those windows.
4. **See it** — optionally **visualize** the sky for a chosen window.

The design keeps each stage a separate, composable tool joined by a shared JSON
schema, so pieces can be built, swapped, and regionalized independently.

## 2. Architecture & layering

`astro` is already moving toward Unix-style composition: every CLI command emits
clean JSON and accepts `--timezone`/`now`. The planner leans into that — it is an
**app-level orchestrator that composes CLI JSON and external data tools**, not a
monolith.

| Layer | Responsibility | Cadence |
|---|---|---|
| **libastro** (pure lib) | ephemeris-computed astronomy only; no I/O, no feeds | — |
| **`astro` CLI** | thin JSON presentation over libastro | on demand |
| **data providers** (separate tools, per region) | weather sky-cover; light pollution | dynamic (hourly) vs static (yearly) |
| **planner / orchestrator** (app-level) | join astronomy + feeds → ranked windows | on demand |
| **renderer** (separate consumer) | JSON → SVG/PNG sky chart | on demand |

### Layering decisions

- **The planner is app-level** and composes CLI JSON; **libastro is unchanged.**
- **libastro's charter is a NOVAS-C equivalent** — positions, reductions, and the
  derived phenomena already present (`place`, `equ2hor`, sidereal time,
  `horizon_events`, `lunar_phases`, `apsides`, `tropical_moments`). It does **not**
  absorb planner, scoring, or feed logic, and it does not grow speculatively.
- **Library growth is extraction-driven (rule-of-three).** New functionality lives
  at the app/CLI layer until the *same* functionality is needed by multiple
  targets; only then is it extracted into a **new library layer** (a separate lib
  target, distinct from the NOVAS-C-equivalent core), driven by demonstrated reuse
  rather than anticipation. This governs even new *pure astronomy* — conjunctions,
  eclipses, a moon-illumination function — all start app-side.

### Why separate tools, not features

Weather and light-pollution data are **region-specific** and have **very
different refresh cadences**. Making each a standalone tool that emits the shared
schema means a user in the EU can drop in an EU weather source without touching
`astro`, and the slow-moving light-pollution map is computed rarely and cached.
The schema — not a plugin API — is the contract.

## 3. The JSON interchange schema (the keystone)

Everything hangs off a small set of provider-agnostic JSON contracts. Design
these first; they are what let the tools evolve independently. (All times are
ISO-8601 with a numeric offset, matching the CLI's existing `--timezone`
output.)

### `Site` — an observing location
```json
{ "lat": 47.688247, "lon": -122.398059, "elevation_m": 100, "timezone": "America/Los_Angeles" }
```

### `IntervalSeries` — the unifying environmental time-series
Darkness, moon presence, and cloud cover are all expressed as one shape — either
**samples** (a value at instants; natural for weather) or **intervals** (start/end
windows; natural for darkness / moon-up). The planner resamples between them.
```json
{
  "quantity": "cloud_cover_pct",          // | "moon_altitude_deg" | "dark" | ...
  "site": { "lat": 47.688, "lon": -122.398, "elevation_m": 100, "timezone": "..." },
  "source": "open-meteo",                 // | "nws" | "met-norway" | "astro"
  "generated_at": "2026-08-07T00:00:00Z",
  "unit": "percent",
  "samples":   [ { "time": "2026-08-07T04:00:00-07:00", "value": 20 } ],
  "intervals": [ { "start": "...", "end": "...", "value": true } ]   // one of samples|intervals
}
```

### `ViewingWindow` — the planner's result
Grows field-by-field as increments land (darkness first, then weather, then a
score, then matched targets). Optional fields are present only when computed.
```json
{
  "site": { "...": "..." },
  "windows": [
    {
      "start": "2026-08-07T21:58-07:00",
      "end":   "2026-08-08T03:12-07:00",
      "duration_hours": 5.23,
      "limiting": { "sun_max_alt_deg": -18.5, "moon_up": false, "moon_illumination": 0.12 },
      "cloud_cover_pct": { "min": 5, "mean": 18, "max": 55 },   // when weather is joined
      "score": 0.78,                                            // when scored
      "targets": [ { "object": "Saturn", "best_alt_deg": 34.1 } ] // when phenomena matched
    }
  ]
}
```

### `CandidateLocations` — light-pollution scouting
```json
{
  "center": { "lat": 47.688, "lon": -122.398 }, "radius_km": 120,
  "candidates": [
    { "lat": 47.35, "lon": -121.05, "distance_km": 82, "bortle": 3, "sqm_mag_arcsec2": 21.6 }
  ]
}
```

## 4. Cadence tiers & the provider abstraction

- **Weather = hot, per-request.** Region-specific sources behind one schema:
  **NWS** `api.weather.gov` gridpoint `skyCover` % (US, free, no key, needs a
  User-Agent); **Open-Meteo** `cloud_cover` (global, free — a pragmatic single
  provider); **MET Norway (Yr)** for the EU. Each emits an `IntervalSeries` with
  `quantity: "cloud_cover_pct"`.
- **Light pollution = cold, cache-and-reuse.** A static map, refreshed rarely,
  produced by a separate low-cadence tool that emits `CandidateLocations`.

## 5. Phenomena taxonomy

Splitting phenomena by *how they're computed* scopes the effort correctly:

- **Ephemeris-computed** (app-level first, per the extraction policy):
  conjunctions / appulses (the root of `d(angular separation)/dt = 0`),
  alignments, and solar / lunar eclipses (Moon–Sun separation vs semidiameters,
  or Moon-in-Earth-shadow geometry). These follow the same root-finding pattern
  as `apsides`, but start in the app/CLI layer over `place`; they graduate to a
  shared library layer (promoting a Brent-style root-finder at that point) only
  once a second target needs them.
- **Almanac data** (an app-level table): meteor showers — debris streams aren't
  in DE440. A small reference file (radiant RA/Dec, peak date, ZHR) plus "is the
  radiant up during the dark window."
- **Separate orbital models** (their own tools): artificial satellites (TLE +
  SGP4) and Near-Earth Objects (JPL Small-Body elements). **Not** DE440.
  libastro's frame/time code can still turn a supplied position into look-angles,
  but the propagation lives outside.

## 6. Domain reality-checks

Documented so they aren't rediscovered the hard way:

- **Moon impact is about altitude, not just phase.** A 90%-lit Moon *below the
  horizon* is fine; a 40% Moon high up wrecks deep-sky viewing. The real metric
  is sky-brightness ≈ `f(illuminated fraction, moon altitude, target
  separation)` — the Krisciunas & Schaefer (1991) model for realism. The MVP
  definition of "dark" is simpler: **Sun below the chosen twilight AND Moon down
  (or below a set altitude)**.
- **Darkness may not exist.** At ~47.7°N there is no astronomical darkness for
  weeks around the summer solstice. The windows engine must report "never fully
  dark" gracefully, not return empty.
- **Prefer real light-pollution data** — VIIRS DNB night-lights composites
  (NOAA) or the World Atlas of Artificial Night Sky Brightness (Falchi 2016, →
  Bortle scale) — over a population-density proxy, which is a weak stand-in when
  a better static dataset already exists.

## 7. Visualization

Start with **SVG** (vector): an azimuthal projection centered on an alt/az with
**FOV = zoom**, bodies drawn at their correct angular radius, and optional
overlays — alt/az grid, ecliptic, constellation lines (a data file). It is a
**consumer of `astro sky --json`**, kept out of the core. A photorealistic raster
mode (textures, Moon-phase shading, atmospheric distortion near the horizon) is a
much larger, later effort.

## 8. Increment roadmap

Ordered; each is independently pickup-able. Nothing here is built yet.

1. **Dark + moonless windows** *(recommended first)* — an app-level planner that
   composes existing CLI JSON:
   `astro rise sun … --horizon astronomical|civil --json` (night intervals) ∩
   `astro rise moon … --horizon moon --json` (moon-up intervals), plus moon
   illumination from `astro moon --json`, over N days → `ViewingWindow` JSON.
   No external services. Establishes `Site` / `IntervalSeries` / `ViewingWindow`.
2. **Weather join** — define the provider schema; implement one provider tool
   (Open-Meteo global, or NWS US); the planner folds `cloud_cover_pct` into each
   window.
3. **Scoring & ranking** — `score = f(dark duration, moon penalty, 1 − cloud)`;
   rank windows; surface the best times.
4. **Light-pollution location optimizer** — a static provider tool; given
   center + radius, return darker candidate sites; feed a chosen site back into
   steps 1–3.
5. **Phenomena expansion** — conjunctions / eclipses (app-level; library-extract
   if reused), a meteor-shower almanac, then satellites / NEOs (separate tools);
   match to windows.
6. **Sky renderer** — SVG from `astro sky --json`; overlays.

### What each increment can reuse today

The dark-window MVP is almost entirely assembled from existing pieces. Current
CLI JSON contracts to build on:

- `astro rise <body> … --horizon <h> --json` → array of
  `{kind, time, jd_tt, altitude_deg, azimuth_deg}`. `--horizon` already offers
  `civil|nautical|astronomical` twilight and a `moon` convention.
- `astro moon <t> --json` (snapshot) →
  `{time, mode, phase, illuminated_fraction, phase_angle_deg, elongation_deg,
  age_deg, distance_km}`.
- `astro sky <t> --observer … --json` →
  `{observer:[lat,lon,h], objects:[{object, ra_hours, dec_deg, distance_*,
  hp_mag?, altitude_deg, azimuth_deg}]}` with `--min-alt` / `--max-mag` filters.

All accept `--timezone` (IANA / `local` / `utc`) and `now`. The only genuinely
new logic in increment 1 is **interval intersection** over these outputs.

## 9. Open questions

To settle when each increment is picked up:

- Darkness definition per target class (naked-eye vs deep-sky → civil vs
  astronomical twilight; moon-altitude threshold).
- The exact moon-brightness model (simple "moon down" vs Krisciunas–Schaefer).
- Whether conjunctions/eclipses stay app-level or, once reused, become a new
  library layer — and whether that layer is libastro or a companion lib.
- Separate repos vs `astro` subcommands for the provider tools.
- Deep-sky (Messier/NGC) catalog source, since only Hipparcos stars exist today.
