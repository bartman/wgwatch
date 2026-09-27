#pragma once

#include <cstddef>
#include <optional>
#include <string>

enum class SortKey {
  Native,    // wg order
  Endpoint,  // endpoint ip
  Allowed,   // allowed ips
  Mru,       // most recent handshake first
  Lru,       // least recent handshake first
  RxBytes,
  TxBytes,
  Bytes,  // rx + tx bytes
  RxRate,
  TxRate,
  Rate,  // rx + tx rate
};

enum class PlotMode {
  Line,  // braille line plot
  Bar,   // unicode block bar plot
};

// Parsed command-line options. All inputs are validated by parse_cli;
// nothing unscrubbed reaches a shell command.
struct CliOptions {
  bool show_keys = false;
  bool hide_inactive = false;  // --hide-inactive, 'i' toggles
  double update_sec = 1.0;
  std::string interface = "all";
  std::optional<std::string> remote;
  SortKey sort = SortKey::Mru;
  PlotMode plot_mode = PlotMode::Line;  // 'p' toggles
  std::size_t theme = 0;                // 't' cycles wtheme::kThemes
  int verbose = 0;  // -v: debug, -vv: trace
  std::optional<std::string> log_file;  // --log: redirect spdlog here
};

// Thrown for -h/--help (main prints usage(), exits 0).
struct HelpRequested {};
// Thrown for --sort help (main prints sort_help(), exits 0).
struct SortHelpRequested {};

CliOptions parse_cli(int argc, char* argv[]);
std::string usage();      // full --help text
std::string sort_help();  // sort type list for --sort help
std::string sort_name(SortKey s);
SortKey next_sort(SortKey s);  // cycle order for the 's' key
std::string plot_name(PlotMode m);
PlotMode next_plot(PlotMode m);  // toggle for the 'p' key
