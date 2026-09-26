#pragma once

#include <cstdint>
#include <string>

#include <fmt/format.h>

// Display formatting (header-only). Tables/units are constexpr; the
// format functions need fmt at runtime so they cannot be constexpr.
namespace wfmt {

inline constexpr const char* kUnits[] = {"B", "KB", "MB", "GB", "TB"};

inline std::string fmt_bytes(uint64_t b) {
  double v = static_cast<double>(b);
  int u = 0;
  while (v >= 1024.0 && u < 4) {
    v /= 1024.0;
    ++u;
  }
  return fmt::format("{:.2f} {}", v, kUnits[u]);
}

inline std::string fmt_rate(double r) {
  double v = r;
  int u = 0;
  while (v >= 1024.0 && u < 4) {
    v /= 1024.0;
    ++u;
  }
  return fmt::format("{:.2f} {}/s", v, kUnits[u]);
}

// sec is an age in seconds (negative = clock skew -> "never").
inline std::string fmt_ago(double sec) {
  if (sec < 0.0) return "never";
  const long s = static_cast<long>(sec);
  if (s < 60) return fmt::format("{} seconds ago", s);
  if (s < 3600) return fmt::format("{} minutes ago", s / 60);
  return fmt::format("{} hours ago", s / 3600);
}

// Window width for the status bar ("1 second", "90 seconds").
inline std::string fmt_span(double sec) {
  long s = static_cast<long>(sec + 0.5);
  if (s < 0) s = 0;
  if (s == 1) return "1 second";
  return fmt::format("{} seconds", s);
}

}  // namespace wfmt
