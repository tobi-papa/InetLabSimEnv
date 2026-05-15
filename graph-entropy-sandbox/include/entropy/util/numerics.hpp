#pragma once
#include <cmath>

namespace entropy::util {

// Returns p * ln(p), with the convention 0 * ln(0) := 0.
// Precondition: p >= 0.
inline double xlogx(double p) noexcept {
    if (p <= 0.0) return 0.0;
    return p * std::log(p);
}

// Returns ln(p). Returns 0.0 when p <= 0 (caller is responsible for
// guarding p == 0 via the 0*ln(0) = 0 convention at the call site).
inline double safe_log(double p) noexcept {
    if (p <= 0.0) return 0.0;
    return std::log(p);
}

} // namespace entropy::util
