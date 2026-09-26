#pragma once

#include <unistd.h>

// Privilege helpers (header-only). sudo/doas lookup is a shell snippet
// resolved inside the loop shell, so remote hosts need no extra round-trip.
namespace priv {

inline bool is_root() { return ::geteuid() == 0; }

inline const char* sudo_detect_snippet() {
  return "$(command -v sudo doas 2>/dev/null | head -n1)";
}

}  // namespace priv
