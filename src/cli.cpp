#include "cli.hpp"

#include <regex>
#include <spdlog/spdlog.h>
#include <stdexcept>

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
         "  --show-keys                 show keys (default hide them)\n";
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

CliOptions parse_cli(int argc, char* argv[]) {
  CliOptions o;
  const std::regex update_re(R"(^[0-9]+(\.[0-9]+)?$)");
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
    } else if (a == "--verbose") {
      ++o.verbose;
    } else if (std::regex_match(a, v_bundle_re)) {
      o.verbose += static_cast<int>(a.size()) - 1;  // -v=1, -vv=2, …
    } else if (a == "--log") {
      o.log_file = need_value(i, "--log");
    } else if (a == "-u" || a == "--update") {
      const std::string v = need_value(i, "--update");
      if (!std::regex_match(v, update_re))
        throw std::invalid_argument("invalid --update '" + v +
                                    "': expected number in [0.1,3600]");
      const double d = std::stod(v);
      if (d < 0.1 || d > 3600.0)
        throw std::invalid_argument("invalid --update '" + v +
                                    "': expected range [0.1,3600]");
      o.update_sec = d;
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
      if (v == "native")
        o.sort = SortKey::Native;
      else if (v == "endpoint")
        o.sort = SortKey::Endpoint;
      else if (v == "allowed")
        o.sort = SortKey::Allowed;
      else if (v == "mru")
        o.sort = SortKey::Mru;
      else if (v == "lru")
        o.sort = SortKey::Lru;
      else if (v == "rx-bytes")
        o.sort = SortKey::RxBytes;
      else if (v == "tx-bytes")
        o.sort = SortKey::TxBytes;
      else if (v == "bytes")
        o.sort = SortKey::Bytes;
      else if (v == "rx-rate")
        o.sort = SortKey::RxRate;
      else if (v == "tx-rate")
        o.sort = SortKey::TxRate;
      else if (v == "rate")
        o.sort = SortKey::Rate;
      else
        throw std::invalid_argument(
            "invalid --sort '" + v +
            "': expected one of native,endpoint,allowed,mru,lru,rx-bytes,"
            "tx-bytes,bytes,rx-rate,tx-rate,rate (or 'help')");
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
