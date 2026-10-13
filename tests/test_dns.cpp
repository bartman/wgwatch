#include <gtest/gtest.h>

#include "../src/dns.hpp"
#include "../src/ui.hpp"

namespace {

TEST(DnsSplit, SingleKeepsMask) {
  EXPECT_EQ(wdns::split_allowed_ips("10.255.0.2/32"),
            std::vector<std::string>{"10.255.0.2/32"});
}

TEST(DnsSplit, CommaSeparatedTrimsAndDropsEmpties) {
  EXPECT_EQ(wdns::split_allowed_ips("10.0.0.2/32, fd00::2/128 ,"),
            (std::vector<std::string>{"10.0.0.2/32", "fd00::2/128"}));
  EXPECT_TRUE(wdns::split_allowed_ips("").empty());
}

TEST(DnsHost, StripsMask) {
  EXPECT_EQ(wdns::host_of("10.0.0.2/32"), "10.0.0.2");
  EXPECT_EQ(wdns::host_of("fd00::2/128"), "fd00::2");
  EXPECT_EQ(wdns::host_of("10.0.0.2"), "10.0.0.2");
}

TEST(DnsCache, MissKicksOffOnceThenInjectResolves) {
  wdns::ReverseDnsCache cache;
  EXPECT_FALSE(cache.lookup("10.255.0.2").has_value());
  EXPECT_EQ(cache.size(), 1u);
  // In-progress ("") still reports nullopt without spawning again.
  EXPECT_FALSE(cache.lookup("10.255.0.2").has_value());
  EXPECT_EQ(cache.size(), 1u);
  cache.inject("10.255.0.2", "peerhost");
  const auto got = cache.lookup("10.255.0.2");
  ASSERT_TRUE(got.has_value());
  EXPECT_EQ(*got, "peerhost");
}

TEST(DnsCache, EmptyMeansFailed) {
  wdns::ReverseDnsCache cache;
  cache.inject("192.0.2.1", "");
  EXPECT_FALSE(cache.lookup("192.0.2.1").has_value());
}

TEST(DnsResolve, BlockingLocalhostHasNameGarbageDoesNot) {
  // 127.0.0.1 resolves via /etc/hosts (no network); garbage never does.
  EXPECT_FALSE(wdns::reverse_resolve_blocking("127.0.0.1").empty());
  EXPECT_TRUE(wdns::reverse_resolve_blocking("not-an-ip").empty());
}

TEST(DnsResolveAll, MixedResolvedAndPending) {
  wdns::ReverseDnsCache cache;
  cache.inject("127.0.0.1", "localhost");
  const auto out =
      wdns::resolve_all("127.0.0.1/32,192.0.2.1/32", cache);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].token, "127.0.0.1/32");
  ASSERT_TRUE(out[0].name.has_value());
  EXPECT_EQ(*out[0].name, "localhost");
  EXPECT_EQ(out[1].token, "192.0.2.1/32");
  EXPECT_FALSE(out[1].name.has_value());
}

TEST(AllowedIpRuns, NameBrightEqualsIpDim) {
  const cpptui::Color bright = cpptui::Color::White();
  const cpptui::Color dim = cpptui::Color::Black();
  const std::vector<wdns::ResolvedIp> ips = {
      {"127.0.0.1/32", std::string("localhost")},
      {"192.0.2.1/32", std::nullopt},
  };
  const auto runs = allowed_ip_text_runs(ips, bright, dim);
  ASSERT_EQ(runs.size(), 6u);
  EXPECT_EQ(runs[0].text, "(");
  EXPECT_EQ(runs[1].text, "localhost");
  EXPECT_TRUE(runs[1].fg.has_value());
  EXPECT_EQ(runs[2].text, "=127.0.0.1/32");
  EXPECT_EQ(runs[3].text, ",");
  EXPECT_EQ(runs[4].text, "192.0.2.1/32");
  EXPECT_EQ(runs[5].text, ")");
  // Dim runs keep the dim color, name run keeps bright.
  EXPECT_EQ(runs[2].fg, std::optional<cpptui::Color>(dim));
  EXPECT_EQ(runs[1].fg, std::optional<cpptui::Color>(bright));
}

// Plain-text walk of a widget subtree.
void collect_labels(const std::shared_ptr<cpptui::Widget>& w,
                    std::string& acc) {
  if (auto lab = std::dynamic_pointer_cast<cpptui::Label>(w)) {
    acc += lab->get_text();
    acc += "\n";
  }
  if (auto c = std::dynamic_pointer_cast<cpptui::Container>(w))
    for (auto& k : c->get_children()) collect_labels(k, acc);
}

TEST(DnsUi, SecondLineShowsNameEqualsIp) {
  cpptui::App app;
  CliOptions opts;
  RateTracker tracker;
  Ui ui(app, opts, tracker);
  ui.dns_cache().inject("10.255.0.2", "peerhost");
  WgFrame f;
  f.tstamp = 1001.0;
  f.ifaces.push_back(WgIface{"wr0", "iface-priv", "iface-pub", 51820});
  WgPeer p;
  p.iface = "wr0";
  p.pubkey = "peer0";
  p.endpoint = "198.51.100.1:1234";
  p.allowed_ips = "10.255.0.2/32";
  p.handshake = static_cast<uint64_t>(std::time(nullptr));
  p.rx = 200;
  p.tx = 200;
  f.peers.push_back(p);
  ui.refresh(f);
  std::string acc;
  collect_labels(ui.root(), acc);
  EXPECT_NE(acc.find("(peerhost=10.255.0.2/32)"), std::string::npos);
}

}  // namespace
