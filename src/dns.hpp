#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Reverse-DNS cache for peer AllowedIPs display.
//
// Table maps bare IP (no /mask) -> hostname. Key absent kicks off a
// background lookup and inserts "" (in-progress); "" also means the lookup
// finished without a name. lookup() never blocks: it returns the name when
// known, nullopt otherwise. Display prints (name=IP/mask) when known, plain
// (IP/mask) while pending/failed.
//
// WireGuard's dump joins multiple AllowedIPs with commas
// (e.g. "10.0.0.2/32,fd00::2/128"); every entry is resolved independently.
// This header is UI-agnostic: resolve_all() returns per-token data and the
// caller (ui.cpp) assigns colors, so both peer-box lines share one path.
namespace wdns {

// "10.0.0.2/32" -> "10.0.0.2"; no '/' -> whole token.
std::string host_of(const std::string& ip_mask);

// Split AllowedIPs on ',' and trim ASCII whitespace; drops empties.
std::vector<std::string> split_allowed_ips(const std::string& allowed_ips);

// Blocking PTR lookup, "" on any failure. Runs only on the background
// thread, never on the UI thread.
std::string reverse_resolve_blocking(const std::string& ip);

class ReverseDnsCache {
 public:
  ReverseDnsCache();
  // Name when resolved, nullopt when pending/failed (kicks off one
  // background thread per unseen IP). Never blocks.
  std::optional<std::string> lookup(const std::string& ip);
  // Test seam / pre-seed: force a mapping (pass "" to mark failed).
  void inject(const std::string& ip, const std::string& name);
  size_t size() const;

 private:
  struct State {
    mutable std::mutex m;
    std::unordered_map<std::string, std::string> table;
  };
  std::shared_ptr<State> st_;
};

// One AllowedIPs token plus its resolved name (nullopt = pending/failed).
// resolve_all() runs lookup() per token host, so first sighting kicks off
// the background thread as a side effect.
struct ResolvedIp {
  std::string token;
  std::optional<std::string> name;
};

std::vector<ResolvedIp> resolve_all(const std::string& allowed_ips,
                                     ReverseDnsCache& cache);

}  // namespace wdns
