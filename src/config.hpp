#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <spdlog/spdlog.h>

#include "cli.hpp"
#include "theme.hpp"

// Persistent UI options in ~/.config/wgwatch/config, one `key = value`
// per line:
//
//   update = 1
//   sort = mru
//   plot = line
//   theme = github-dark
//   inactive = hide
//   public_keys = hidden
//
// remote/interface are never persisted (always local/all). Unknown keys
// and bad values warn and are ignored, so hand edits can't break startup.
namespace wconfig {

inline std::string trim(const std::string& s) {
  const std::string::size_type b = s.find_first_not_of(" \t\r");
  if (b == std::string::npos) return "";
  const std::string::size_type e = s.find_last_not_of(" \t\r");
  return s.substr(b, e - b + 1);
}

// Applies file content over o (CLI parses over the result, so flags win).
inline void read_config(std::istream& in, CliOptions& o) {
  std::string line;
  int lineno = 0;
  while (std::getline(in, line)) {
    ++lineno;
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;
    const std::string::size_type eq = line.find('=');
    if (eq == std::string::npos) {
      spdlog::warn("wgwatch: config line {}: no '=', skipping", lineno);
      continue;
    }
    const std::string key = trim(line.substr(0, eq));
    const std::string val = trim(line.substr(eq + 1));
    try {
      if (key == "update") {
        o.update_sec = update_from_string(val);
      } else if (key == "sort") {
        o.sort = sort_from_name(val);
      } else if (key == "plot") {
        o.plot_mode = plot_from_name(val);
      } else if (key == "theme") {
        std::size_t idx = 0;
        if (!wtheme::theme_index(val, idx)) throw std::invalid_argument(val);
        o.theme = idx;
      } else if (key == "inactive") {
        if (val == "hide")
          o.hide_inactive = true;
        else if (val == "show")
          o.hide_inactive = false;
        else
          throw std::invalid_argument(val);
      } else if (key == "public_keys") {
        if (val == "show")
          o.show_keys = true;
        else if (val == "hide")
          o.show_keys = false;
        else
          throw std::invalid_argument(val);
      } else {
        spdlog::warn("wgwatch: config line {}: unknown key '{}', skipping",
                     lineno, key);
      }
    } catch (const std::exception&) {
      spdlog::warn("wgwatch: config line {}: bad value '{}', skipping",
                   lineno, val);
    }
  }
}

inline void write_config(std::ostream& out, const CliOptions& o) {
  out << "update = " << o.update_sec << "\n";
  out << "sort = " << sort_name(o.sort) << "\n";
  out << "plot = " << plot_name(o.plot_mode) << "\n";
  out << "theme = " << wtheme::theme_at(o.theme).name << "\n";
  out << "inactive = " << (o.hide_inactive ? "hide" : "show") << "\n";
  out << "public_keys = " << (o.show_keys ? "show" : "hide") << "\n";
}

inline std::string config_path() {
  const char* home = std::getenv("HOME");
  if (home == nullptr || home[0] == '\0') return "";
  return std::string(home) + "/.config/wgwatch/config";
}

// Defaults overlaid with the file (missing file = first run, no noise).
inline CliOptions load_config() {
  CliOptions o;
  const std::string path = config_path();
  if (path.empty()) return o;
  std::ifstream in(path);
  if (!in) return o;
  read_config(in, o);
  return o;
}

// Atomic: staging file in the same directory, then rename over.
// Warn-only: called from UI key handlers, must never break the app.
inline void save_config(const CliOptions& o) {
  const std::string path = config_path();
  if (path.empty()) {
    spdlog::warn("wgwatch: no HOME, config not saved");
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(
      std::filesystem::path(path).parent_path(), ec);
  if (ec) {
    spdlog::warn("wgwatch: cannot create config dir: {}", ec.message());
    return;
  }
  const std::string tmp = path + ".next";
  {
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) {
      spdlog::warn("wgwatch: cannot write {}", tmp);
      return;
    }
    write_config(out, o);
    out.flush();
    if (!out) {
      spdlog::warn("wgwatch: failed writing {}", tmp);
      return;
    }
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec) spdlog::warn("wgwatch: cannot replace {}: {}", path, ec.message());
}

}  // namespace wconfig
