#include "parser.hpp"

#include <spdlog/spdlog.h>

#include <string>

namespace {

bool is_timestamp(std::string_view line) {
  if (line.empty()) return false;
  bool dot = false;
  for (const char c : line) {
    if (c == '.') {
      if (dot) return false;
      dot = true;
    } else if (c < '0' || c > '9') {
      return false;
    }
  }
  return dot;
}

std::vector<std::string_view> split_lines(std::string_view s) {
  std::vector<std::string_view> out;
  size_t i = 0;
  while (i <= s.size()) {
    size_t j = s.find('\n', i);
    if (j == std::string_view::npos) j = s.size();
    std::string_view line = s.substr(i, j - i);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    out.push_back(line);
    i = j + 1;
  }
  return out;
}

// Real `wg show all dump` separates columns with tabs, but tolerate any
// whitespace run (the checked-in example uses spaces). No field value
// itself contains whitespace, so this is lossless.
std::vector<std::string_view> split_ws(std::string_view s) {
  std::vector<std::string_view> out;
  size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    if (i >= s.size()) break;
    size_t j = i;
    while (j < s.size() && s[j] != ' ' && s[j] != '\t') ++j;
    out.push_back(s.substr(i, j - i));
    i = j;
  }
  return out;
}
}  // namespace

WgFrame parse_frame(std::string_view block, std::string_view filter) {
  WgFrame f;
  for (const std::string_view line : split_lines(block)) {
    if (line.empty()) continue;
    if (is_timestamp(line)) {
      if (f.tstamp == 0.0) {
        try {
          f.tstamp = std::stod(std::string(line));
        } catch (...) {
        }
      }
      continue;
    }
    const auto cols = split_ws(line);
    if (cols.size() == 5) {
      if (filter != "all" && cols[0] != filter) continue;
      WgIface ni;
      ni.name = std::string(cols[0]);
      ni.privkey = std::string(cols[1]);
      if (cols[3] == "off") {
        ni.port = 0;
      } else {
        try {
          ni.port = static_cast<uint16_t>(std::stoi(std::string(cols[3])));
        } catch (...) {
          spdlog::warn("wgwatch: bad iface port, skipping row");
          continue;
        }
      }
      f.ifaces.push_back(std::move(ni));
    } else if (cols.size() == 9) {
      if (filter != "all" && cols[0] != filter) continue;
      WgPeer np;
      np.iface = std::string(cols[0]);
      np.pubkey = std::string(cols[1]);
      np.psk = std::string(cols[2]);
      np.endpoint = std::string(cols[3]);
      np.allowed_ips = std::string(cols[4]);
      try {
        np.handshake = std::stoull(std::string(cols[5]));
        np.rx = std::stoull(std::string(cols[6]));
        np.tx = std::stoull(std::string(cols[7]));
      } catch (...) {
        spdlog::warn("wgwatch: bad peer counters, skipping row");
        continue;
      }
      f.peers.push_back(std::move(np));
    } else {
      spdlog::warn("wgwatch: skipping malformed row ({} cols)", cols.size());
    }
  }
  spdlog::trace("parse: {}B filter={} -> {} ifaces {} peers t={:.3f}",
                block.size(), filter, f.ifaces.size(), f.peers.size(),
                f.tstamp);
  return f;
}
