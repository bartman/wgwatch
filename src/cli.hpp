#pragma once

#include <cstddef>
#include <optional>
#include <string>

enum class SortKey {
  Native,    // wg order
  Endpoint,  // endpoint ip
  Allowed,   // allowed ips
  Mru,       // most recent handshake/rx activity first
  Lru,       // least recent handshake/rx activity first
  Handshake,  // most recent handshake first (ignores rx activity)
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

// Lazy reorder delay: a newly-sorted peer order is only adopted once it
// persists this long, so one-frame spikes don't bounce rows around.
enum class LazyMode {
  Off,  // adopt every fresh order immediately
  S3,   // adopt after 3s of stability
  S5,   // adopt after 5s of stability
};

// Parsed command-line options. All inputs are validated by parse_cli;
// nothing unscrubbed reaches a shell command.
struct CliOptions {
  bool show_keys = false;
  bool hide_inactive = false;  // --hide-inactive, 'i' toggles
  double update_sec = 1.0;
  std::string interface = "all";
  std::optional<std::string> remote;
  std::string command = "wg";  // --command: wg binary path, never persisted
  SortKey sort = SortKey::Mru;
  LazyMode lazy = LazyMode::S3;  // 'l' cycles off/3s/5s
  PlotMode plot_mode = PlotMode::Line;  // 'p' toggles
  std::size_t theme = 0;                // 't' cycles wtheme::kThemes
  int verbose = 0;  // -v: debug, -vv: trace
  std::optional<std::string> log_file;  // --log: redirect spdlog here
};

// Thrown for -h/--help (main prints usage(), exits 0).
struct HelpRequested {};
// Thrown for --sort help (main prints sort_help(), exits 0).
struct SortHelpRequested {};
// Thrown for --version (main prints version_string(), exits 0).
struct VersionRequested {};

// Flags present on argv overwrite base; absent ones keep base values.
// Lets the config file provide defaults that CLI overrides.
CliOptions parse_cli(int argc, char* argv[],
                      CliOptions base = CliOptions{});
std::string usage();           // full --help text
std::string version_string();  // "wgwatch X.Y.Z"
std::string sort_help();       // sort type list for --sort help
std::string sort_name(SortKey s);
SortKey next_sort(SortKey s);  // cycle order for the 's' key
std::string plot_name(PlotMode m);
PlotMode next_plot(PlotMode m);  // toggle for the 'p' key
std::string lazy_name(LazyMode m);
LazyMode next_lazy(LazyMode m);  // cycle for the 'l' key
// Settle delay in seconds (0 when off).
double lazy_delay_sec(LazyMode m);
// Validated value parser; throws std::invalid_argument on bad input.
LazyMode lazy_from_name(const std::string& v);

// Validated value parsers shared by parse_cli and the config file.
// Throw std::invalid_argument on bad input.
SortKey sort_from_name(const std::string& v);
PlotMode plot_from_name(const std::string& v);
double update_from_string(const std::string& v);
