#ifndef ASTRO_ERROR_HPP
#define ASTRO_ERROR_HPP

#include <string_view>

namespace astro {

// Replaces NOVAS-C's per-function `short int` return-code convention. Paired
// with std::expected throughout the public API.
enum class EphError {
  ok = 0,
  not_implemented,     // scaffold placeholder
  file_not_found,      // ephem_open == 1
  bad_header,          // ephem_open 2..10
  unsupported_denum,   // ephem_open == 11
  epoch_out_of_range,  // state == 2
  io_error,            // state == 1
  unknown_constant,
  invalid_argument,
  no_convergence,      // light-time iteration failed to converge
};

constexpr std::string_view to_string(EphError e) noexcept {
  switch (e) {
    case EphError::ok:                 return "ok";
    case EphError::not_implemented:    return "not implemented";
    case EphError::file_not_found:     return "ephemeris file not found";
    case EphError::bad_header:         return "malformed ephemeris header";
    case EphError::unsupported_denum:  return "unsupported DE number";
    case EphError::epoch_out_of_range: return "epoch out of ephemeris range";
    case EphError::io_error:           return "ephemeris I/O error";
    case EphError::unknown_constant:   return "unknown constant name";
    case EphError::invalid_argument:   return "invalid argument";
    case EphError::no_convergence:     return "iteration did not converge";
  }
  return "unknown error";
}

}  // namespace astro

#endif  // ASTRO_ERROR_HPP
