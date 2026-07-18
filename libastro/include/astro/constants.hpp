#ifndef ASTRO_CONSTANTS_HPP
#define ASTRO_CONSTANTS_HPP

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace astro {

// The named constant block. DE440 carries NCON=645 name/value pairs; NOVAS-C
// discards them, and >400 names spill from CNAM into CNAM2 after LPT (research
// doc 1.2, 1.6). Ephemeris::open() parses the names (CNAM + CNAM2) and the
// record-2 values into this map; test/unit/test_constants.cpp checks every pair
// against the con440.c transcription (bit-for-bit).
class Constants {
 public:
  Constants() = default;
  explicit Constants(std::unordered_map<std::string, double> values)
      : values_(std::move(values)) {}

  std::optional<double> get(std::string_view name) const {
    if (auto it = values_.find(std::string(name)); it != values_.end())
      return it->second;
    return std::nullopt;
  }

  // Convenience accessors for the widely-used constants. Return nullopt if the
  // ephemeris did not carry the key.
  std::optional<double> au_km() const { return get("AU"); }
  std::optional<double> earth_moon_ratio() const { return get("EMRAT"); }
  std::optional<double> speed_of_light_km_s() const { return get("CLIGHT"); }
  std::optional<double> earth_radius_km() const { return get("RE"); }

  std::size_t size() const noexcept { return values_.size(); }
  bool empty() const noexcept { return values_.empty(); }

  // All name/value pairs, for enumeration (e.g. listing every constant). The
  // order is unspecified (hash order); sort by name if you need a stable list.
  const std::unordered_map<std::string, double>& entries() const noexcept {
    return values_;
  }

 private:
  std::unordered_map<std::string, double> values_;
};

}  // namespace astro

#endif  // ASTRO_CONSTANTS_HPP
