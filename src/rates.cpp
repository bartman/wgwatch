#include "rates.hpp"

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <spdlog/spdlog.h>

void RateTracker::ingest(const WgFrame& f) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const WgPeer& p : f.peers) {
    const std::string key = p.iface + "|" + p.pubkey;
    PeerState& st = peers_[key];
    if (st.last_t >= 0.0 && f.tstamp > st.last_t) {
      const double dt = f.tstamp - st.last_t;
      st.rx_rate = (p.rx >= st.last_rx)
                       ? static_cast<double>(p.rx - st.last_rx) / dt
                       : 0.0;
      st.tx_rate = (p.tx >= st.last_tx)
                       ? static_cast<double>(p.tx - st.last_tx) / dt
                       : 0.0;
    } else {
      st.rx_rate = 0.0;
      st.tx_rate = 0.0;
    }
    if (st.n < st.rx_hist.size()) {
      st.rx_hist[st.n] = st.rx_rate;
      st.tx_hist[st.n] = st.tx_rate;
      st.t_hist[st.n] = f.tstamp;
      ++st.n;
    } else {
      for (std::size_t i = 1; i < st.rx_hist.size(); ++i) {
        st.rx_hist[i - 1] = st.rx_hist[i];
        st.tx_hist[i - 1] = st.tx_hist[i];
        st.t_hist[i - 1] = st.t_hist[i];
      }
      st.rx_hist.back() = st.rx_rate;
      st.tx_hist.back() = st.tx_rate;
      st.t_hist.back() = f.tstamp;
    }
    st.cur = p;
    st.last_rx = p.rx;
    st.last_tx = p.tx;
    st.last_t = f.tstamp;
  }
  spdlog::trace("rates: ingest t={:.3f} peers={} tracked={}", f.tstamp,
                f.peers.size(), peers_.size());
}

double RateTracker::history_span() const {
  std::lock_guard<std::mutex> lock(mutex_);
  double span = 0.0;
  for (const auto& [key, st] : peers_) {
    if (st.n >= 2) span = std::max(span, st.t_hist[st.n - 1] - st.t_hist[0]);
  }
  return span < 0.0 ? 0.0 : span;
}

namespace {

struct EpKey {
  int cls = 2;  // 0 = IPv4, 1 = other host, 2 = no endpoint (sorts last)
  uint32_t v4 = 0;
  std::string host;
};

EpKey endpoint_key(const std::string& endpoint) {
  std::string h;
  if (!endpoint.empty() && endpoint != "(none)") {
    if (endpoint.front() == '[') {
      const std::string::size_type c = endpoint.find(']');
      h = (c == std::string::npos) ? "" : endpoint.substr(1, c - 1);
    } else {
      const std::string::size_type c = endpoint.rfind(':');
      h = (c == std::string::npos) ? endpoint : endpoint.substr(0, c);
    }
  }
  if (h.empty()) return {};
  unsigned a = 0, b = 0, c = 0, d = 0;
  char extra = 0;
  if (std::sscanf(h.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) == 4 &&
      a < 256 && b < 256 && c < 256 && d < 256)
    return {0, (a << 24) | (b << 16) | (c << 8) | d, h};
  return {1, 0, h};
}

double rate_of(const std::map<std::string, PeerState>& states,
               const WgPeer& p, bool rx) {
  const auto it = states.find(p.iface + "|" + p.pubkey);
  if (it == states.end()) return 0.0;
  return rx ? it->second.rx_rate : it->second.tx_rate;
}

}  // namespace

std::vector<const WgPeer*> sort_peers(
    const WgFrame& frame, const std::map<std::string, PeerState>& states,
    SortKey sort) {
  std::vector<const WgPeer*> out;
  out.reserve(frame.peers.size());
  for (const WgPeer& p : frame.peers) out.push_back(&p);
  switch (sort) {
    case SortKey::Native:
      break;
    case SortKey::Endpoint:
      std::stable_sort(out.begin(), out.end(),
                       [](const WgPeer* x, const WgPeer* y) {
                         const EpKey a = endpoint_key(x->endpoint);
                         const EpKey b = endpoint_key(y->endpoint);
                         if (a.cls != b.cls) return a.cls < b.cls;
                         if (a.v4 != b.v4) return a.v4 < b.v4;
                         return a.host < b.host;
                       });
      break;
    case SortKey::Allowed:
      std::stable_sort(out.begin(), out.end(),
                       [](const WgPeer* x, const WgPeer* y) {
                         return x->allowed_ips < y->allowed_ips;
                       });
      break;
    case SortKey::Mru:
    case SortKey::Lru: {
      const bool mru = sort == SortKey::Mru;
      std::stable_sort(out.begin(), out.end(),
                       [mru](const WgPeer* x, const WgPeer* y) {
                         // never (0) counts as oldest: last for mru, first
                         // for lru.
                         const int64_t kx =
                             x->handshake == 0
                                 ? -1
                                 : static_cast<int64_t>(x->handshake);
                         const int64_t ky =
                             y->handshake == 0
                                 ? -1
                                 : static_cast<int64_t>(y->handshake);
                         return mru ? kx > ky : kx < ky;
                       });
      break;
    }
    case SortKey::RxBytes:
    case SortKey::TxBytes:
    case SortKey::Bytes: {
      std::stable_sort(out.begin(), out.end(),
                       [sort](const WgPeer* x, const WgPeer* y) {
                         const auto tot = [sort](const WgPeer& p) {
                           if (sort == SortKey::Bytes) return p.rx + p.tx;
                           return sort == SortKey::RxBytes ? p.rx : p.tx;
                         };
                         return tot(*x) > tot(*y);
                       });
      break;
    }
    case SortKey::RxRate:
    case SortKey::TxRate:
    case SortKey::Rate: {
      std::stable_sort(out.begin(), out.end(),
                       [&](const WgPeer* x, const WgPeer* y) {
                         const auto tot = [&](const WgPeer& p) {
                           if (sort == SortKey::Rate)
                             return rate_of(states, p, true) +
                                    rate_of(states, p, false);
                           return rate_of(states, p,
                                          sort == SortKey::RxRate);
                         };
                         return tot(*x) > tot(*y);
                       });
      break;
    }
  }
  return out;
}
