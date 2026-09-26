#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Parsed `wg show all dump` rows. Tab-split (never space-split):
// 5 columns = interface, 9 columns = peer. Anything else is warn+skip.
// Empty input yields an empty frame; parse_frame never throws.
struct WgPeer {
  std::string iface;
  std::string pubkey;
  std::string psk;
  std::string endpoint;
  std::string allowed_ips;
  uint64_t handshake = 0;
  uint64_t rx = 0;
  uint64_t tx = 0;
};

struct WgIface {
  std::string name;
  std::string privkey;
  std::string pubkey;
  uint16_t port = 0;  // "off" -> 0
};

struct WgFrame {
  double tstamp = 0.0;
  bool stale = false;
  std::vector<WgIface> ifaces;
  std::vector<WgPeer> peers;
};

WgFrame parse_frame(std::string_view block, std::string_view filter = "all");
