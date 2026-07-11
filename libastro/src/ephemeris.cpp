#include "astro/ephemeris.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// std::mdspan: native <mdspan> when the toolchain ships it, else the vendored
// Kokkos reference impl (its <experimental/mdspan> exposes std::mdspan). See
// cmake/mdspan.cmake.
#if defined(LIBASTRO_NATIVE_MDSPAN)
#  include <mdspan>
#else
#  include <experimental/mdspan>
#endif

// Layer 0 implementation. `open()` parses record 1; `state()` reproduces
// eph_manager.c's planet_ephemeris/state/interpolate (research doc 1.4, 1.5),
// but statelessly (per-call polynomial evaluation, no file-scope globals) and
// viewing each body's coefficient block as a rank-3 std::mdspan
// [subinterval][component][coeff] (research doc 1.3, 3.3).
//
// Assumption: JPL "Linux" ephemerides are little-endian IEEE-754; both target
// platforms (x86_64, aarch64) are little-endian, so fields are read directly.
static_assert(std::endian::native == std::endian::little,
              "libastro currently assumes a little-endian host; add byte-swaps "
              "for big-endian support.");

namespace astro {

namespace {

// Header field sizes, in the on-disk order eph_manager.c reads them.
constexpr std::size_t kTtlBytes = 3 * 84;    // 252
constexpr std::size_t kCnamBytes = 400 * 6;  // 2400

std::size_t record_length_for_denum(int denum) {
  switch (denum) {
    case 200:
      return 6608;
    case 403: case 405: case 421:
    case 430: case 431: case 440: case 441:
      return 8144;
    case 404: case 406:
      return 5824;
    default:
      return 0;  // unsupported
  }
}

std::string trim(const char* p, std::size_t n) {
  std::size_t end = n;
  while (end > 0 && (p[end - 1] == ' ' || p[end - 1] == '\0')) --end;
  return std::string(p, end);
}

// Largest coefficient count per component across DE440's groups is 14 (Mercury);
// this bounds the local Chebyshev basis arrays without heap traffic.
constexpr int kMaxCoeff = 32;

// Break a double into integer and (non-negative) fractional parts, matching
// eph_manager.c:split exactly (needed for bit-comparable epoch handling).
void split(double tt, double fr[2]) {
  fr[0] = static_cast<double>(static_cast<long>(tt));
  fr[1] = tt - fr[0];
  if (tt >= 0.0 || fr[1] == 0.0) return;
  fr[0] -= 1.0;
  fr[1] += 1.0;
}

// Differentiate and interpolate one 3-component body from its Chebyshev block.
//   buf : first coefficient of the body within the record
//   t0  : fractional time within the whole record span, [0, 1)
//   t1  : record span length in output time units (days for AU, s for km)
//   ncf : coefficients per component;  na : sub-intervals in the record
// Reproduces eph_manager.c:interpolate; the coefficient block is addressed as a
// rank-3 std::mdspan with layout [subinterval][component][coeff] (layout_right,
// stride: coeff=1, component=ncf, subinterval=3*ncf).
void interpolate(const double* buf, double t0, double t1, int ncf, int na,
                 double position[3], double velocity[3]) {
  using Extents =
      std::extents<std::size_t, std::dynamic_extent, 3, std::dynamic_extent>;
  const std::mdspan<const double, Extents> coeff(
      buf, Extents(static_cast<std::size_t>(na),
                   static_cast<std::size_t>(ncf)));

  const double dna = static_cast<double>(na);
  const double dt1 = static_cast<double>(static_cast<long>(t0));
  const double temp = dna * t0;
  const long l = static_cast<long>(temp - dt1);          // sub-interval index
  const double tc = 2.0 * (std::fmod(temp, 1.0) + dt1) - 1.0;  // in [-1, 1]
  const double twot = tc + tc;

  // Chebyshev polynomials T_j(tc).
  std::array<double, kMaxCoeff> pc{};
  pc[0] = 1.0;
  pc[1] = tc;
  for (int i = 2; i < ncf; ++i) pc[i] = twot * pc[i - 1] - pc[i - 2];

  for (int i = 0; i < 3; ++i) {
    double s = 0.0;
    for (int j = ncf - 1; j >= 0; --j) s += pc[j] * coeff[l, i, j];
    position[i] = s;
  }

  // Derivative basis dT_j/dtc.
  std::array<double, kMaxCoeff> vc{};
  vc[0] = 0.0;
  vc[1] = 1.0;
  vc[2] = 2.0 * twot;
  for (int i = 3; i < ncf; ++i)
    vc[i] = twot * vc[i - 1] + 2.0 * pc[i - 1] - vc[i - 2];

  const double vfac = (2.0 * dna) / t1;
  for (int i = 0; i < 3; ++i) {
    double s = 0.0;
    for (int j = ncf - 1; j >= 1; --j) s += vc[j] * coeff[l, i, j];
    velocity[i] = s * vfac;
  }
}

}  // namespace

struct Ephemeris::Impl {
  std::ifstream file;
  Header header;
  Constants constants;
  // Single-record cache (analogue of eph_manager's BUFFER/NRL). Confined to the
  // instance rather than file-scope; a single Ephemeris is therefore not safe
  // for concurrent state() calls -- use one instance per thread.
  std::vector<double> buffer;
  long cached_record = -1;

  // Load 1-based physical record `nr` into `buffer` (no-op if already cached).
  bool load_record(long nr) {
    if (nr == cached_record) return true;
    const auto rec = static_cast<std::streamoff>(nr - 1) *
                     static_cast<std::streamoff>(header.record_length);
    file.clear();
    file.seekg(rec, std::ios::beg);
    file.read(reinterpret_cast<char*>(buffer.data()),
              static_cast<std::streamsize>(header.record_length));
    if (!file) return false;
    cached_record = nr;
    return true;
  }

  // Barycentric state of one raw group (state numbering: 0=Mercury..2=EMB..
  // 9=Moon(geo)..10=Sun) at split TDB `jed`. Mirrors eph_manager.c:state.
  std::expected<void, EphError> read_state(int body, const double jed[2],
                                           Units units, double pos[3],
                                           double vel[3]) {
    const double ss0 = header.jd_begin;
    const double ss1 = header.jd_end;
    const double step = header.days_per_record;

    double interval;   // record span in output time units
    double aufac;      // km -> output length unit
    if (units == Units::km) {
      interval = step * 86400.0;
      aufac = 1.0;
    } else {
      interval = step;
      aufac = 1.0 / header.au_km;
    }

    // Resolve the split epoch to an integer day + fraction (eph_manager:state).
    double jd[4];
    split(jed[0] - 0.5, &jd[0]);
    split(jed[1], &jd[2]);
    jd[0] += jd[2] + 0.5;
    jd[1] += jd[3];
    split(jd[1], &jd[2]);
    jd[0] += jd[2];

    if (jd[0] < ss0 || (jd[0] + jd[3]) > ss1)
      return std::unexpected(EphError::epoch_out_of_range);

    long nr = static_cast<long>((jd[0] - ss0) / step) + 3;
    if (jd[0] == ss1) nr -= 2;
    const double t0 =
        ((jd[0] - (static_cast<double>(nr - 3) * step + ss0)) + jd[3]) / step;

    if (!load_record(nr)) return std::unexpected(EphError::io_error);

    const GroupLayout& g = header.groups[static_cast<std::size_t>(body)];
    interpolate(&buffer[static_cast<std::size_t>(g.offset - 1)], t0, interval,
                g.n_coeff, g.n_subintervals, pos, vel);
    for (int i = 0; i < 3; ++i) {
      pos[i] *= aufac;
      vel[i] *= aufac;
    }
    return {};
  }
};

Ephemeris::Ephemeris() : impl_(std::make_unique<Impl>()) {}
Ephemeris::Ephemeris(Ephemeris&&) noexcept = default;
Ephemeris& Ephemeris::operator=(Ephemeris&&) noexcept = default;
Ephemeris::~Ephemeris() = default;

std::expected<Ephemeris, EphError> Ephemeris::open(
    const std::filesystem::path& path) {
  Ephemeris eph;
  Impl& s = *eph.impl_;

  s.file.open(path, std::ios::binary);
  if (!s.file) return std::unexpected(EphError::file_not_found);

  char ttl[kTtlBytes];
  char cnam[kCnamBytes];
  double ss[3];
  std::int32_t ncon;
  double au;
  double emrat;
  std::int32_t ipt[12][3];
  std::int32_t denum;
  std::int32_t lpt[3];

  auto read = [&](void* dst, std::size_t n) {
    s.file.read(static_cast<char*>(dst), static_cast<std::streamsize>(n));
    return static_cast<bool>(s.file);
  };

  if (!read(ttl, sizeof ttl)) return std::unexpected(EphError::bad_header);
  if (!read(cnam, sizeof cnam)) return std::unexpected(EphError::bad_header);
  if (!read(ss, sizeof ss)) return std::unexpected(EphError::bad_header);
  if (!read(&ncon, sizeof ncon)) return std::unexpected(EphError::bad_header);
  if (!read(&au, sizeof au)) return std::unexpected(EphError::bad_header);
  if (!read(&emrat, sizeof emrat)) return std::unexpected(EphError::bad_header);
  if (!read(ipt, sizeof ipt)) return std::unexpected(EphError::bad_header);
  if (!read(&denum, sizeof denum)) return std::unexpected(EphError::bad_header);
  if (!read(lpt, sizeof lpt)) return std::unexpected(EphError::bad_header);

  const std::size_t reclen = record_length_for_denum(denum);
  if (reclen == 0) return std::unexpected(EphError::unsupported_denum);

  Header& h = s.header;
  h.title = trim(ttl, 84);  // first of the three 84-char lines
  h.jd_begin = ss[0];
  h.jd_end = ss[1];
  h.days_per_record = ss[2];
  h.denum = denum;
  h.n_constants = ncon;
  h.au_km = au;
  h.earth_moon_ratio = emrat;
  h.record_length = reclen;

  for (int g = 0; g < 12; ++g)
    h.groups[static_cast<std::size_t>(g)] = {ipt[g][0], ipt[g][1], ipt[g][2]};
  h.groups[12] = {lpt[0], lpt[1], lpt[2]};  // librations

  std::error_code ec;
  const auto bytes = std::filesystem::file_size(path, ec);
  h.record_count = ec ? 0 : static_cast<std::size_t>(bytes) / reclen;

  s.buffer.resize(reclen / sizeof(double));

  // Named constants (research doc 1.2, 1.6): the first 400 names are in record
  // 1's CNAM; when NCON > 400 the remainder (CNAM2) follows LPT -- which is
  // exactly where the stream sits now -- and the NCON values start record 2.
  if (ncon > 0) {
    auto name_at = [](const char* p) {
      std::size_t n = 6;
      while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\0')) --n;
      return std::string(p, n);
    };

    std::vector<std::string> names;
    names.reserve(static_cast<std::size_t>(ncon));
    const int n_rec1 = ncon < 400 ? ncon : 400;
    for (int i = 0; i < n_rec1; ++i) names.push_back(name_at(cnam + i * 6));
    if (ncon > 400) {
      std::vector<char> cnam2(static_cast<std::size_t>(ncon - 400) * 6);
      if (!read(cnam2.data(), cnam2.size()))
        return std::unexpected(EphError::bad_header);
      for (int i = 0; i < ncon - 400; ++i)
        names.push_back(name_at(cnam2.data() + i * 6));
    }

    std::vector<double> values(static_cast<std::size_t>(ncon));
    s.file.clear();
    s.file.seekg(static_cast<std::streamoff>(reclen), std::ios::beg);
    if (!read(values.data(), values.size() * sizeof(double)))
      return std::unexpected(EphError::bad_header);

    std::unordered_map<std::string, double> cmap;
    cmap.reserve(static_cast<std::size_t>(ncon));
    for (int i = 0; i < ncon; ++i)
      cmap.emplace(names[static_cast<std::size_t>(i)],
                   values[static_cast<std::size_t>(i)]);
    s.constants = Constants(std::move(cmap));
  }

  return eph;
}

const Header& Ephemeris::header() const noexcept { return impl_->header; }
const Constants& Ephemeris::constants() const noexcept {
  return impl_->constants;
}

bool Ephemeris::covers(TdbInstant t) const noexcept {
  const double jd = t.jd.value();
  return jd >= impl_->header.jd_begin && jd <= impl_->header.jd_end;
}

std::expected<StateVector, EphError> Ephemeris::state(
    Point target, Point center, TdbInstant t, Units units) const {
  const int tgt = static_cast<int>(target);
  const int ctr = static_cast<int>(center);

  if (tgt == ctr) return StateVector{{}, {}, units};

  const double jed[2] = {t.jd.whole, t.jd.frac};

  // Port of eph_manager.c:planet_ephemeris. Point numbering matches the raw
  // state numbering except that index 2 denotes Earth (raw group 2 is the
  // Earth-Moon barycenter) and 9 denotes the geocentric Moon; both are
  // reconstructed via EMRAT below (research doc 1.5). 11 = SSB, 12 = EMB.
  constexpr int kEarth = 2, kMoon = 9, kSsb = 11, kEmb = 12;

  const bool do_moon = (tgt == kEarth) || (ctr == kEarth);
  const bool do_earth =
      (tgt == kMoon) || (ctr == kMoon) || (tgt == kEmb) || (ctr == kEmb);

  double pos_earth[3] = {}, vel_earth[3] = {};  // raw group 2 == EMB
  double pos_moon[3] = {}, vel_moon[3] = {};     // raw group 9 == geocentric Moon

  Impl& s = *impl_;
  if (do_earth) {
    if (auto r = s.read_state(kEarth, jed, units, pos_earth, vel_earth); !r)
      return std::unexpected(r.error());
  }
  if (do_moon) {
    if (auto r = s.read_state(kMoon, jed, units, pos_moon, vel_moon); !r)
      return std::unexpected(r.error());
  }

  double tp[3] = {}, tv[3] = {}, cp[3] = {}, cv[3] = {};

  if (tgt == kSsb) {
    // barycenter -> zero state
  } else if (tgt == kEmb) {
    for (int i = 0; i < 3; ++i) { tp[i] = pos_earth[i]; tv[i] = vel_earth[i]; }
  } else if (auto r = s.read_state(tgt, jed, units, tp, tv); !r) {
    return std::unexpected(r.error());
  }

  if (ctr == kSsb) {
    // barycenter -> zero state
  } else if (ctr == kEmb) {
    for (int i = 0; i < 3; ++i) { cp[i] = pos_earth[i]; cv[i] = vel_earth[i]; }
  } else if (auto r = s.read_state(ctr, jed, units, cp, cv); !r) {
    return std::unexpected(r.error());
  }

  StateVector out;
  out.units = units;

  // Earth<->Moon direct pairs, then the single-endpoint EMB/Moon corrections.
  const double emr1 = 1.0 + header().earth_moon_ratio;
  if (tgt == kEarth && ctr == kMoon) {
    for (int i = 0; i < 3; ++i) { out.position[i] = -cp[i]; out.velocity[i] = -cv[i]; }
    return out;
  } else if (tgt == kMoon && ctr == kEarth) {
    for (int i = 0; i < 3; ++i) { out.position[i] = tp[i]; out.velocity[i] = tv[i]; }
    return out;
  } else if (tgt == kEarth) {
    for (int i = 0; i < 3; ++i) { tp[i] -= pos_moon[i] / emr1; tv[i] -= vel_moon[i] / emr1; }
  } else if (ctr == kEarth) {
    for (int i = 0; i < 3; ++i) { cp[i] -= pos_moon[i] / emr1; cv[i] -= vel_moon[i] / emr1; }
  } else if (tgt == kMoon) {
    for (int i = 0; i < 3; ++i) {
      tp[i] = (pos_earth[i] - tp[i] / emr1) + tp[i];
      tv[i] = (vel_earth[i] - tv[i] / emr1) + tv[i];
    }
  } else if (ctr == kMoon) {
    for (int i = 0; i < 3; ++i) {
      cp[i] = (pos_earth[i] - cp[i] / emr1) + cp[i];
      cv[i] = (vel_earth[i] - cv[i] / emr1) + cv[i];
    }
  }

  for (int i = 0; i < 3; ++i) {
    out.position[i] = tp[i] - cp[i];
    out.velocity[i] = tv[i] - cv[i];
  }
  return out;
}

}  // namespace astro
