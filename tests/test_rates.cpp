#include <gtest/gtest.h>

#include "../src/rates.hpp"

namespace {

WgFrame frame_at(double t, uint64_t rx, uint64_t tx) {
  WgFrame f;
  f.tstamp = t;
  WgPeer p;
  p.iface = "wg0";
  p.pubkey = "k1";
  p.rx = rx;
  p.tx = tx;
  f.peers.push_back(p);
  return f;
}

}  // namespace

TEST(Rates, FirstFrameZero) {
  RateTracker tr;
  tr.ingest(frame_at(100.0, 1000, 2000));
  const auto m = tr.peers();
  ASSERT_EQ(m.count("wg0|k1"), 1u);
  EXPECT_DOUBLE_EQ(m.at("wg0|k1").rx_rate, 0.0);
  EXPECT_DOUBLE_EQ(m.at("wg0|k1").tx_rate, 0.0);
}

TEST(Rates, DeltaOverDt) {
  RateTracker tr;
  tr.ingest(frame_at(100.0, 1000, 2000));
  tr.ingest(frame_at(110.0, 2000, 4000));
  const auto m = tr.peers();
  EXPECT_DOUBLE_EQ(m.at("wg0|k1").rx_rate, 100.0);
  EXPECT_DOUBLE_EQ(m.at("wg0|k1").tx_rate, 200.0);
  EXPECT_EQ(m.at("wg0|k1").n, 2u);
}

TEST(Rates, CounterResetClampsToZero) {
  RateTracker tr;
  tr.ingest(frame_at(100.0, 5000, 6000));
  tr.ingest(frame_at(110.0, 100, 200));  // interface recreated
  const auto m = tr.peers();
  EXPECT_DOUBLE_EQ(m.at("wg0|k1").rx_rate, 0.0);
  EXPECT_DOUBLE_EQ(m.at("wg0|k1").tx_rate, 0.0);
}

TEST(Rates, StaleTimestampYieldsZero) {
  RateTracker tr;
  tr.ingest(frame_at(100.0, 1000, 2000));
  tr.ingest(frame_at(100.0, 9000, 9000));  // dt == 0
  EXPECT_DOUBLE_EQ(tr.peers().at("wg0|k1").rx_rate, 0.0);
}

TEST(Rates, HistorySpanGrowsThenCaps) {
  RateTracker tr;
  EXPECT_DOUBLE_EQ(tr.history_span(), 0.0);
  for (int i = 0; i < 5; ++i) {
    WgFrame f;
    f.tstamp = 100.0 + i;
    WgPeer p;
    p.iface = "wg0";
    p.pubkey = "k";
    p.rx = static_cast<uint64_t>(i);
    f.peers.push_back(p);
    tr.ingest(f);
  }
  EXPECT_DOUBLE_EQ(tr.history_span(), 4.0);
  for (int i = 5; i < 130; ++i) {
    WgFrame f;
    f.tstamp = 100.0 + i;
    WgPeer p;
    p.iface = "wg0";
    p.pubkey = "k";
    p.rx = static_cast<uint64_t>(i);
    f.peers.push_back(p);
    tr.ingest(f);
  }
  // 130 frames at 1s: oldest 10 dropped, window 110..229 frozen at 119s.
  EXPECT_EQ(tr.peers().at("wg0|k").n, 120u);
  EXPECT_DOUBLE_EQ(tr.history_span(), 119.0);
}
