#include "dns.hpp"

#include <thread>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

#include <spdlog/spdlog.h>

namespace wdns {

std::string host_of(const std::string& ip_mask) {
  const auto slash = ip_mask.find('/');
  return slash == std::string::npos ? ip_mask : ip_mask.substr(0, slash);
}

std::vector<std::string> split_allowed_ips(const std::string& allowed_ips) {
  std::vector<std::string> out;
  std::size_t i = 0;
  while (i <= allowed_ips.size()) {
    const auto j = allowed_ips.find(',', i);
    const auto end = j == std::string::npos ? allowed_ips.size() : j;
    // Trim ASCII whitespace around the token.
    std::size_t b = i;
    while (b < end &&
           (allowed_ips[b] == ' ' || allowed_ips[b] == '\t'))
      ++b;
    std::size_t e = end;
    while (e > b &&
           (allowed_ips[e - 1] == ' ' || allowed_ips[e - 1] == '\t'))
      --e;
    if (e > b) out.push_back(allowed_ips.substr(b, e - b));
    if (j == std::string::npos) break;
    i = j + 1;
  }
  return out;
}

std::string reverse_resolve_blocking(const std::string& ip) {
  char host[NI_MAXHOST] = {};
  int rc = -1;
  {
    struct sockaddr_in sin {};
    sin.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip.c_str(), &sin.sin_addr) == 1)
      rc = getnameinfo(reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin),
                       host, sizeof(host), nullptr, 0, NI_NAMEREQD);
  }
  if (rc != 0) {
    struct sockaddr_in6 sin6 {};
    sin6.sin6_family = AF_INET6;
    if (inet_pton(AF_INET6, ip.c_str(), &sin6.sin6_addr) == 1)
      rc = getnameinfo(reinterpret_cast<struct sockaddr*>(&sin6),
                       sizeof(sin6), host, sizeof(host), nullptr, 0,
                       NI_NAMEREQD);
  }
  if (rc != 0) return "";
  // A numeric answer is not a name (happens without NI_NAMEREQD fallback,
  // but belt-and-braces): treat as unresolved.
  if (host == ip) return "";
  return std::string(host);
}

ReverseDnsCache::ReverseDnsCache() : st_(std::make_shared<State>()) {}

std::optional<std::string> ReverseDnsCache::lookup(const std::string& ip) {
  {
    std::lock_guard<std::mutex> lk(st_->m);
    const auto it = st_->table.find(ip);
    if (it != st_->table.end())
      return it->second.empty() ? std::nullopt
                                : std::optional<std::string>(it->second);
    st_->table.emplace(ip, "");
  }
  // First sighting: resolve off-thread; detached, holding only the shared
  // state so a destroyed Ui/cache can't dangle.
  std::thread([st = st_, ip] {
    const std::string name = reverse_resolve_blocking(ip);
    {
      std::lock_guard<std::mutex> lk(st->m);
      st->table[ip] = name;
    }
    spdlog::trace("dns: resolved={} cached={}", name.empty() ? 0 : 1,
                  st->table.size());
  }).detach();
  return std::nullopt;
}

void ReverseDnsCache::inject(const std::string& ip, const std::string& name) {
  std::lock_guard<std::mutex> lk(st_->m);
  st_->table[ip] = name;
}

size_t ReverseDnsCache::size() const {
  std::lock_guard<std::mutex> lk(st_->m);
  return st_->table.size();
}

std::vector<ResolvedIp> resolve_all(const std::string& allowed_ips,
                                     ReverseDnsCache& cache) {
  std::vector<ResolvedIp> out;
  for (auto& tok : split_allowed_ips(allowed_ips))
    out.push_back({tok, cache.lookup(host_of(tok))});
  return out;
}

}  // namespace wdns
