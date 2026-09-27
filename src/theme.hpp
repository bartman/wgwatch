#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

#include "cpptui.hpp"

// Built-in UI themes. Palettes sourced from https://terminalcolors.com
// (Alacritty downloads): background/foreground, bright-black as muted,
// blue as header accent, green/cyan for rx/tx graphs.
namespace wtheme {

struct Theme {
  const char* name;
  const char* bg;
  const char* fg;
  const char* muted;   // plot background, borders, secondary text
  const char* header;  // header + interface lines
  const char* rx;      // rx graph + numbers
  const char* tx;      // tx graph + numbers
  const char* ok;      // fresh handshake
  const char* warn;    // aging handshake
  const char* err;     // stale / never
};

inline constexpr std::array<Theme, 12> kThemes = {{
    // https://terminalcolors.com/themes/catppuccin/mocha/
    {"catppuccin-mocha", "#1e1e2e", "#cdd6f4", "#585b70", "#89b4fa", "#a6e3a1",
     "#94e2d5", "#a6e3a1", "#f9e2af", "#f38ba8"},
    // https://terminalcolors.com/themes/dracula/default/
    {"dracula", "#282a36", "#f8f8f2", "#6272a4", "#bd93f9", "#50fa7b",
     "#8be9fd", "#50fa7b", "#f1fa8c", "#ff5555"},
    // https://terminalcolors.com/themes/nord/default/
    {"nord", "#2e3440", "#d8dee9", "#4c566a", "#81a1c1", "#a3be8c", "#88c0d0",
     "#a3be8c", "#ebcb8b", "#bf616a"},
    // https://terminalcolors.com/themes/gruvbox/dark/
    {"gruvbox-dark", "#282828", "#ebdbb2", "#928374", "#458588", "#98971a",
     "#689d6a", "#98971a", "#d79921", "#cc241d"},
    // https://terminalcolors.com/themes/tokyo-night/default/
    {"tokyo-night", "#1a1b26", "#c0caf5", "#414868", "#7aa2f7", "#9ece6a",
     "#7dcfff", "#9ece6a", "#e0af68", "#f7768e"},
    // https://terminalcolors.com/themes/solarized/dark/
    {"solarized-dark", "#002b36", "#839496", "#586e75", "#268bd2", "#859900",
     "#2aa198", "#859900", "#b58900", "#dc322f"},
    // https://terminalcolors.com/themes/github/dark/
    {"github-dark", "#010409", "#e6edf3", "#6e7681", "#58a6ff", "#3fb950",
     "#39c5cf", "#3fb950", "#d29922", "#ff7b72"},
    // https://terminalcolors.com/themes/shades-of-purple/default/
    {"shades-of-purple", "#1e1e3f", "#ffffff", "#5c5c61", "#7857fe",
     "#3ad900", "#80fcff", "#3ad900", "#fad000", "#e43937"},
    // https://terminalcolors.com/themes/rose-pine/default/
    {"rose-pine", "#1f1d2e", "#e0def4", "#908caa", "#9ccfd8", "#31748f",
     "#ebbcba", "#31748f", "#f6c177", "#eb6f92"},
    // https://terminalcolors.com/themes/kanagawa/wave/
    {"kanagawa-wave", "#1f1f28", "#dcd7ba", "#727169", "#7e9cd8", "#76946a",
     "#6a9589", "#76946a", "#c0a36e", "#c34043"},
    // https://terminalcolors.com/themes/everforest/dark/
    {"everforest-dark", "#2d353b", "#d3c6aa", "#859289", "#7fbbb3", "#a7c080",
     "#83c092", "#a7c080", "#dbbc7f", "#e67e80"},
    // https://terminalcolors.com/themes/one/dark/
    {"one-dark", "#282c34", "#abb2bf", "#5c6370", "#61afef", "#98c379",
     "#56b6c2", "#98c379", "#d19a66", "#e06c75"},
}};
// Name -> index for --theme and the config file. False when unknown.
inline bool theme_index(const char* name, std::size_t& out) {
  for (std::size_t i = 0; i < kThemes.size(); ++i)
    if (std::strcmp(kThemes[i].name, name) == 0) {
      out = i;
      return true;
    }
  return false;
}

inline bool theme_index(const std::string& name, std::size_t& out) {
  return theme_index(name.c_str(), out);
}

inline constexpr std::size_t num_themes() { return kThemes.size(); }

// Wraps around, so the 't' key can cycle with modulo.
inline const Theme& theme_at(std::size_t i) {
  return kThemes[i % kThemes.size()];
}

// "#rrggbb" -> Color. No validation: all inputs are table constants.
inline cpptui::Color color(const char* hex) {
  const auto nyb = [](char c) -> uint8_t {
    if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
    return static_cast<uint8_t>(c - 'A' + 10);
  };
  const uint8_t r =
      static_cast<uint8_t>((nyb(hex[1]) << 4) | nyb(hex[2]));
  const uint8_t g =
      static_cast<uint8_t>((nyb(hex[3]) << 4) | nyb(hex[4]));
  const uint8_t b =
      static_cast<uint8_t>((nyb(hex[5]) << 4) | nyb(hex[6]));
  return {r, g, b, false};
}

inline cpptui::Color color(const std::string& hex) {
  return color(hex.c_str());
}

}  // namespace wtheme
