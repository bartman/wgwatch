#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "../src/ui.hpp"

namespace {

WgPeer named(const std::string& pubkey) {
  WgPeer p;
  p.iface = "wg0";
  p.pubkey = pubkey;
  return p;
}

std::string order_of(const std::vector<const WgPeer*>& v) {
  std::string s;
  for (const WgPeer* p : v) s += p->pubkey;
  return s;
}

struct Four {
  std::vector<WgPeer> store = {named("A"), named("B"), named("C"),
                               named("D")};
  std::vector<const WgPeer*> abcd() {
    return {&store[0], &store[1], &store[2], &store[3]};
  }
  std::vector<const WgPeer*> dabc() {
    return {&store[3], &store[0], &store[1], &store[2]};
  }
};

TEST(LazyStabilizer, OffAdoptsImmediately) {
  Four f;
  OrderStabilizer st;
  EXPECT_EQ(order_of(st.update(f.abcd(), 0.0, 0.0)), "ABCD");
  EXPECT_EQ(order_of(st.update(f.dabc(), 1.0, 0.0)), "DABC");
}

TEST(LazyStabilizer, OneFrameSpikeNeverShown) {
  Four f;
  OrderStabilizer st;
  // T=0..2 trace: D spikes for a single frame, display never moves.
  EXPECT_EQ(order_of(st.update(f.abcd(), 0.0, 3.0)), "ABCD");
  EXPECT_EQ(order_of(st.update(f.dabc(), 1.0, 3.0)), "ABCD");
  EXPECT_EQ(order_of(st.update(f.abcd(), 2.0, 3.0)), "ABCD");
}

TEST(LazyStabilizer, SustainedReorderAdoptsAfterDelay) {
  Four f;
  OrderStabilizer st;
  EXPECT_EQ(order_of(st.update(f.abcd(), 0.0, 3.0)), "ABCD");
  EXPECT_EQ(order_of(st.update(f.dabc(), 1.0, 3.0)), "ABCD");  // 0s pending
  EXPECT_EQ(order_of(st.update(f.dabc(), 3.0, 3.0)), "ABCD");  // 2s pending
  EXPECT_EQ(order_of(st.update(f.dabc(), 4.0, 3.0)), "DABC");  // 3s: adopt
  EXPECT_EQ(order_of(st.update(f.dabc(), 5.0, 3.0)), "DABC");
}

TEST(LazyStabilizer, JoinAndLeaveAdoptImmediately) {
  Four f;
  OrderStabilizer st;
  EXPECT_EQ(order_of(st.update(f.abcd(), 0.0, 5.0)), "ABCD");
  EXPECT_EQ(order_of(st.update(f.dabc(), 1.0, 5.0)), "ABCD");
  WgPeer e = named("E");
  const std::vector<const WgPeer*> with_join = {&f.store[0], &f.store[1],
                                                &f.store[2], &f.store[3], &e};
  EXPECT_EQ(order_of(st.update(with_join, 2.0, 5.0)), "ABCDE");
  const std::vector<const WgPeer*> with_leave = {&f.store[0], &f.store[1],
                                                 &f.store[2]};
  EXPECT_EQ(order_of(st.update(with_leave, 3.0, 5.0)), "ABC");
}

TEST(LazyStabilizer, ResetAdoptsFresh) {
  Four f;
  OrderStabilizer st;
  EXPECT_EQ(order_of(st.update(f.abcd(), 0.0, 5.0)), "ABCD");
  st.reset();  // sort key changed: old displayed order is meaningless
  EXPECT_EQ(order_of(st.update(f.dabc(), 1.0, 5.0)), "DABC");
}

TEST(LazyMode, CycleNamesAndDelays) {
  EXPECT_EQ(next_lazy(LazyMode::Off), LazyMode::S3);
  EXPECT_EQ(next_lazy(LazyMode::S3), LazyMode::S5);
  EXPECT_EQ(next_lazy(LazyMode::S5), LazyMode::Off);
  EXPECT_EQ(lazy_name(LazyMode::Off), "off");
  EXPECT_EQ(lazy_name(LazyMode::S3), "3s");
  EXPECT_EQ(lazy_name(LazyMode::S5), "5s");
  EXPECT_DOUBLE_EQ(lazy_delay_sec(LazyMode::Off), 0.0);
  EXPECT_DOUBLE_EQ(lazy_delay_sec(LazyMode::S3), 3.0);
  EXPECT_DOUBLE_EQ(lazy_delay_sec(LazyMode::S5), 5.0);
  EXPECT_EQ(prev_lazy(LazyMode::Off), LazyMode::S5);
  EXPECT_EQ(prev_lazy(LazyMode::S3), LazyMode::Off);
  EXPECT_EQ(prev_lazy(LazyMode::S5), LazyMode::S3);
  EXPECT_EQ(next_lazy(prev_lazy(LazyMode::S3)), LazyMode::S3);
  EXPECT_EQ(lazy_from_name("off"), LazyMode::Off);
  EXPECT_EQ(lazy_from_name("3s"), LazyMode::S3);
  EXPECT_EQ(lazy_from_name("5s"), LazyMode::S5);
  EXPECT_THROW(lazy_from_name("10s"), std::invalid_argument);
  EXPECT_EQ(CliOptions{}.lazy, LazyMode::S3);  // default on, 3s
}

}  // namespace
