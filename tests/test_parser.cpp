#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>

#include "../src/parser.hpp"

namespace {

std::string read_all(const std::string& path) {
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

}  // namespace

// NOTE: _attic/example-wg-show-all-dump.txt holds 1 iface row + 10 peer
// rows (plan says 9; the file has 10). Tests assert the file's ground truth.
TEST(Parser, FixtureRowCounts) {
  const std::string dump =
      read_all(WG_ATTIC_DIR "/example-wg-show-all-dump.txt");
  ASSERT_FALSE(dump.empty());
  const WgFrame f = parse_frame(dump, "all");
  EXPECT_EQ(f.ifaces.size(), 1u);
  EXPECT_EQ(f.peers.size(), 10u);
}

TEST(Parser, FixtureFields) {
  const std::string dump =
      read_all(WG_ATTIC_DIR "/example-wg-show-all-dump.txt");
  const WgFrame f = parse_frame(dump, "all");
  ASSERT_EQ(f.ifaces.size(), 1u);
  EXPECT_EQ(f.ifaces[0].name, "wg0");
  EXPECT_EQ(f.ifaces[0].port, 51820);
  ASSERT_GE(f.peers.size(), 1u);
  const WgPeer& p = f.peers[0];
  EXPECT_EQ(p.iface, "wg0");
  EXPECT_EQ(p.endpoint, "184.146.158.96:44793");
  EXPECT_EQ(p.allowed_ips, "10.255.0.2/32");
  EXPECT_EQ(p.handshake, 1789913956u);
  EXPECT_EQ(p.rx, 241808u);
  EXPECT_EQ(p.tx, 220984u);
}

TEST(Parser, FilterExact) {
  const std::string dump =
      read_all(WG_ATTIC_DIR "/example-wg-show-all-dump.txt");
  EXPECT_EQ(parse_frame(dump, "wg0").peers.size(), 10u);
  EXPECT_TRUE(parse_frame(dump, "other").peers.empty());
  EXPECT_TRUE(parse_frame(dump, "other").ifaces.empty());
}

TEST(Parser, EmptyNeverThrows) {
  EXPECT_NO_THROW({
    const WgFrame f = parse_frame("", "all");
    EXPECT_TRUE(f.peers.empty());
    EXPECT_TRUE(f.ifaces.empty());
  });
}

TEST(Parser, TimestampCaptured) {
  const WgFrame f = parse_frame("1790389975.123456\n", "all");
  EXPECT_DOUBLE_EQ(f.tstamp, 1790389975.123456);
}
