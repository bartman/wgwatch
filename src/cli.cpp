#include "cli.hpp"

#include <regex>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include "theme.hpp"

std::string usage() {
  return "wgwatch — top-like WireGuard monitor\n"
         "\n"
         "  -h --help                   this help\n"
         "  -r --remote [user@]host     access stats remotely via ssh (default "
         "local)\n"
         "  -u --update <sec>           frequence of display update (default "
         "1)\n"
         "  -i --interface <iface>      which wg* interface to watch (default "
         "all)\n"
         "  -s --sort <type>            how to sort endpoints (use type 'help' "
         "to see types)\n"
         "  -v --verbose                increase log verbosity (repeat for "
         "trace)\n"
         "  --log <file>                write logs to file (default stderr)\n"
         "  --show-keys                 show keys (default hide them)\n"
         "  --hide-inactive             hide peers that never handshook\n"
         "  --plot <type>               plot style: line, bar\n"
         "  --theme <name>              color theme (default catppuccin-mocha)\n"
         "  --inactive <mode>           hide|show peers that never handshook\n"
         "  --public-keys <mode>        show|hide WireGuard keys\n";
}

std::string sort_help() {
  return "sort types:\n"
         "  native      order reported by wg\n"
         "  endpoint    sort by endpoint ip\n"
         "  allowed     sort by allowed ips\n"
         "  mru         most recent handshake first (default)\n"
         "  lru         least recent handshake first\n"
         "  rx-bytes    sort by rx bytes\n"
         "  tx-bytes    sort by tx bytes\n"
         "  bytes       sort by rx+tx bytes\n"
         "  rx-rate     sort by rx rate\n"
         "  tx-rate     sort by tx rate\n"
         "  rate        sort by rx+tx rate\n";
}

std::string sort_name(SortKey s) {
  switch (s) {
    case SortKey::Native:
      return "native";
    case SortKey::Endpoint:
      return "endpoint";
    case SortKey::Allowed:
      return "allowed";
    case SortKey::Mru:
      return "mru";
    case SortKey::Lru:
      return "lru";
    case SortKey::RxBytes:
      return "rx-bytes";
    case SortKey::TxBytes:
      return "tx-bytes";
    case SortKey::Bytes:
      return "bytes";
    case SortKey::RxRate:
      return "rx-rate";
    case SortKey::TxRate:
      return "tx-rate";
    case SortKey::Rate:
      return "rate";
  }
  return "mru";  // unreachable
}

SortKey next_sort(SortKey s) {
  static constexpr SortKey kOrder[] = {
      SortKey::Native,   SortKey::Endpoint, SortKey::Allowed,
      SortKey::Mru,      SortKey::Lru,      SortKey::RxBytes,
      SortKey::TxBytes,  SortKey::Bytes,    SortKey::RxRate,
      SortKey::TxRate,   SortKey::Rate,
  };
  constexpr auto n = sizeof(kOrder) / sizeof(kOrder[0]);
  for (std::size_t i = 0; i < n; ++i)
    if (kOrder[i] == s) return kOrder[(i + 1) % n];
  return SortKey::Mru;
}

std::string plot_name(PlotMode m) {
  switch (m) {
    case PlotMode::Line:
      return "line";
    case PlotMode::Bar:
      return "bar";
  }
  return "line";  // unreachable
}

PlotMode next_plot(PlotMode m) {
  return m == PlotMode::Line ? PlotMode::Bar : PlotMode::Line;
}

SortKey sort_from_name(const std::string& v) {
  if (v == "native") return SortKey::Native;
  if (v == "endpoint") return SortKey::Endpoint;
  if (v == "allowed") return SortKey::Allowed;
  if (v == "mru") return SortKey::Mru;
  if (v == "lru") return SortKey::Lru;
  if (v == "rx-bytes") return SortKey::RxBytes;
  if (v == "tx-bytes") return SortKey::TxBytes;
  if (v == "bytes") return SortKey::Bytes;
  if (v == "rx-rate") return SortKey::RxRate;
  if (v == "tx-rate") return SortKey::TxRate;
  if (v == "rate") return SortKey::Rate;
  throw std::invalid_argument(
      "invalid --sort '" + v +
      "': expected one of native,endpoint,allowed,mru,lru,rx-bytes,"
      "tx-bytes,bytes,rx-rate,tx-rate,rate (or 'help')");
}

PlotMode plot_from_name(const std::string& v) {
  if (v == "line") return PlotMode::Line;
  if (v == "bar") return PlotMode::Bar;
  throw std::invalid_argument("invalid --plot '" + v +
                              "': expected one of line,bar");
}

double update_from_string(const std::string& v) {
  static const std::regex update_re(R"(^[0-9]+(\.[0-9]+)?$)");
  if (!std::regex_match(v, update_re))
    throw std::invalid_argument("invalid --update '" + v +
                                "': expected number in [0.1,3600]");
  const double d = std::stod(v);
  if (d < 0.1 || d > 3600.0)
    throw std::invalid_argument("invalid --update '" + v +
                                "': expected range [0.1,3600]");
  return d;
}

CliOptions parse_cli(int argc, char* argv[], CliOptions base) {
  CliOptions o = base;
  const std::regex iface_re(R"(^[A-Za-z0-9_=+.-]{1,15}$)");
  const std::regex remote_re(R"(^([A-Za-z0-9._-]+@)?[A-Za-z0-9._-]+$)");
  const std::regex v_bundle_re(R"(^-v+$)");
  auto need_value = [&](int& i, const char* flag) -> std::string {
    if (i + 1 >= argc)
      throw std::invalid_argument(std::string("missing value for ") + flag);
    return argv[++i];
  };
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "-h" || a == "--help") {
      throw HelpRequested{};
    } else if (a == "--show-keys") {
      o.show_keys = true;
    } else if (a == "--hide-inactive") {
      o.hide_inactive = true;
    } else if (a == "--verbose") {
      ++o.verbose;
    } else if (std::regex_match(a, v_bundle_re)) {
      o.verbose += static_cast<int>(a.size()) - 1;  // -v=1, -vv=2, …
    } else if (a == "--log") {
      o.log_file = need_value(i, "--log");
    } else if (a == "-u" || a == "--update") {
      o.update_sec = update_from_string(need_value(i, "--update"));
    } else if (a == "-i" || a == "--interface") {
      const std::string v = need_value(i, "--interface");
      if (!std::regex_match(v, iface_re))
        throw std::invalid_argument("invalid --interface '" + v + "'");
      o.interface = v;
    } else if (a == "-r" || a == "--remote") {
      const std::string v = need_value(i, "--remote");
      if (!std::regex_match(v, remote_re))
        throw std::invalid_argument("invalid --remote '" + v + "'");
      o.remote = v;
    } else if (a == "-s" || a == "--sort") {
      const std::string v = need_value(i, "--sort");
      if (v == "help") throw SortHelpRequested{};
      o.sort = sort_from_name(v);
    } else if (a == "--plot") {
      o.plot_mode = plot_from_name(need_value(i, "--plot"));
    } else if (a == "--theme") {
      const std::string v = need_value(i, "--theme");
      std::size_t idx = 0;
      if (!wtheme::theme_index(v, idx))
        throw std::invalid_argument("invalid --theme '" + v +
                                    "': expected a theme name");
      o.theme = idx;
    } else if (a == "--inactive") {
      const std::string v = need_value(i, "--inactive");
      if (v == "hide")
        o.hide_inactive = true;
      else if (v == "show")
        o.hide_inactive = false;
      else
        throw std::invalid_argument("invalid --inactive '" + v +
                                    "': expected one of hide,show");
    } else if (a == "--public-keys") {
      const std::string v = need_value(i, "--public-keys");
      if (v == "show")
        o.show_keys = true;
      else if (v == "hide")
        o.show_keys = false;
      else
        throw std::invalid_argument("invalid --public-keys '" + v +
                                    "': expected one of show,hide");
    } else {
      throw std::invalid_argument("unknown option '" + a + "'");
    }
  }
  spdlog::debug("cli: update={:.3f}s interface={} remote={} sort={} keys={} "
                "verbose={} log={}",
                o.update_sec, o.interface,
                o.remote ? *o.remote : std::string("local"),
                sort_name(o.sort), o.show_keys ? "shown" : "hidden",
                o.verbose,
                o.log_file ? *o.log_file : std::string("stderr"));
  return o;
}
