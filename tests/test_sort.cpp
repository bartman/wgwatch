#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "../src/rates.hpp"

namespace {

WgPeer peer(const std::string& key, const std::string& ep,
            const std::string& allowed, uint64_t hs, uint64_t rx,
            uint64_t tx) {
  WgPeer p;
  p.iface = "wg0";
  p.pubkey = key;
  p.endpoint = ep;
  p.allowed_ips = allowed;
  p.handshake = hs;
  p.rx = rx;
  p.tx = tx;
  return p;
}

WgFrame three() {
  WgFrame f;
  f.tstamp = 100.0;
  f.peers = {
      peer("A", "10.0.0.2:1", "10.0.0.2/32", 100, 100, 900),
      peer("B", "9.0.0.1:1", "10.0.0.1/32", 200, 300, 100),
      peer("C", "(none)", "10.0.0.3/32", 0, 50, 50),
  };
  return f;
}

std::string order_of(const std::vector<const WgPeer*>& v) {
  std::string s;
  for (const WgPeer* p : v) s += p->pubkey;
  return s;
}

}  // namespace

TEST(Sort, NativeKeepsWgOrder) {
  const WgFrame f = three();
  EXPECT_EQ(order_of(sort_peers(f, {}, SortKey::Native)), "ABC");
}

TEST(Sort, EndpointNumericNoneLast) {
  const WgFrame f = three();
  // Numeric: 9.x < 10.x (lexicographic would order 10.x first); (none) last.
  EXPECT_EQ(order_of(sort_peers(f, {}, SortKey::Endpoint)), "BAC");
}

TEST(Sort, AllowedLexicographic) {
  const WgFrame f = three();
  EXPECT_EQ(order_of(sort_peers(f, {}, SortKey::Allowed)), "BAC");
}

TEST(Sort, MruNeverLast) {
  const WgFrame f = three();
  EXPECT_EQ(order_of(sort_peers(f, {}, SortKey::Mru)), "BAC");
}

TEST(Sort, LruNeverFirst) {
  const WgFrame f = three();
  EXPECT_EQ(order_of(sort_peers(f, {}, SortKey::Lru)), "CAB");
}

TEST(Sort, RxBytesDesc) {
  const WgFrame f = three();
  EXPECT_EQ(order_of(sort_peers(f, {}, SortKey::RxBytes)), "BAC");
}

TEST(Sort, TxBytesDesc) {
  const WgFrame f = three();
  EXPECT_EQ(order_of(sort_peers(f, {}, SortKey::TxBytes)), "ABC");
}

TEST(Sort, RxRateDesc) {
  RateTracker tr;
  WgFrame f1;
  f1.tstamp = 100.0;
  f1.peers = {
      peer("A", "10.0.0.2:1", "10.0.0.2/32", 100, 100, 0),
      peer("B", "9.0.0.1:1", "10.0.0.1/32", 200, 300, 0),
      peer("C", "(none)", "10.0.0.3/32", 0, 0, 0),
  };
  WgFrame f2 = f1;
  f2.tstamp = 110.0;
  f2.peers[0].rx = 200;  // +100 -> 10/s
  f2.peers[1].rx = 300;  // +0   -> 0/s
  f2.peers[2].rx = 50;   // +50  -> 5/s
  tr.ingest(f1);
  tr.ingest(f2);
  EXPECT_EQ(order_of(sort_peers(f2, tr.peers(), SortKey::RxRate)), "ACB");
}

TEST(Sort, BytesDesc) {
  const WgFrame f = three();
  EXPECT_EQ(order_of(sort_peers(f, {}, SortKey::Bytes)), "ABC");
}

TEST(Sort, RateDesc) {
  RateTracker tr;
  WgFrame f1;
  f1.tstamp = 100.0;
  f1.peers = {
      peer("A", "10.0.0.2:1", "10.0.0.2/32", 100, 100, 0),
      peer("B", "9.0.0.1:1", "10.0.0.1/32", 200, 300, 0),
      peer("C", "(none)", "10.0.0.3/32", 0, 0, 0),
  };
  WgFrame f2 = f1;
  f2.tstamp = 110.0;
  f2.peers[0].rx = 200;  // total 10/s
  f2.peers[1].rx = 300;  // total 0/s
  f2.peers[2].rx = 50;   // total 5/s
  tr.ingest(f1);
  tr.ingest(f2);
  EXPECT_EQ(order_of(sort_peers(f2, tr.peers(), SortKey::Rate)), "ACB");
}

TEST(Sort, NextSortCycles) {
  EXPECT_EQ(next_sort(SortKey::Mru), SortKey::Lru);
  EXPECT_EQ(next_sort(SortKey::Rate), SortKey::Native);
  EXPECT_EQ(sort_name(SortKey::Bytes), "bytes");
  EXPECT_EQ(sort_name(SortKey::Mru), "mru");
}
