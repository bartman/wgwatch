#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "cli.hpp"
#include "parser.hpp"

// Per-peer rate state keyed by "iface|pubkey". First frame (or clock going
// backwards) yields 0 rates; counter resets clamp to 0.
//
// Forgetting policy: fixed 120-sample ring per peer (value + frame time,
// oldest-first). Past 120 samples the oldest drops on every ingest, so the
// displayed window covers at most the last ~120 frames. Peers absent from
// later frames are never pruned from the map (the UI only renders peers
// present in the latest frame, so they are invisible but retained).
struct PeerState {
  WgPeer cur;
  double rx_rate = 0.0;
  double tx_rate = 0.0;
  std::array<double, 120> rx_hist{};
  std::array<double, 120> tx_hist{};
  std::array<double, 120> t_hist{};
  std::size_t n = 0;
  uint64_t last_rx = 0;
  uint64_t last_tx = 0;
  double last_t = -1.0;
};

// Mean and peak over the retained rate window (what the plots show).
// Pure: unit-tested.
struct WindowStats {
  double avg = 0.0;
  double peak = 0.0;
};

inline WindowStats window_stats(const std::array<double, 120>& hist,
                                std::size_t n) {
  WindowStats s;
  if (n == 0) return s;
  double sum = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    sum += hist[i];
    s.peak = std::max(s.peak, hist[i]);
  }
  s.avg = sum / static_cast<double>(n);
  return s;
}

class RateTracker {
 public:
  void ingest(const WgFrame& f);

  // Copy under lock: sampler thread ingests while the UI thread reads.
  std::map<std::string, PeerState> peers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return peers_;
  }

  // Widest retained window across peers, in seconds (0 when empty).
  // Freezes once every peer's ring is full; jitters with frame timing.
  double history_span() const;

 private:
  std::map<std::string, PeerState> peers_;
  mutable std::mutex mutex_;
};

// Peer display order for the UI (stable: ties keep wg order).
// Byte/rate sorts run largest-first; endpoint/allowed/mru/lru ascend with
// never-handshook (0) treated as oldest.
std::vector<const WgPeer*> sort_peers(
    const WgFrame& frame, const std::map<std::string, PeerState>& states,
    SortKey sort);
