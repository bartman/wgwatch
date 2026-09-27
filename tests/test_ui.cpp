#include <gtest/gtest.h>

#include "../src/cli.hpp"
#include "../src/ui.hpp"

namespace {

std::array<double, 120> ramp(std::size_t n) {
  std::array<double, 120> h{};
  for (std::size_t i = 0; i < n; ++i) h[i] = static_cast<double>(i);
  return h;
}

TEST(PlotHeight, EmptyPeersMax) { EXPECT_EQ(plot_height_for(24, 0, 0), 4); }

TEST(PlotHeight, RoomyClampsToFour) {
  // 2 + 1 iface + 1 peer * 5 = 8 fixed; (24-8)/1 = 16 -> clamp 4.
  EXPECT_EQ(plot_height_for(24, 1, 1), 4);
}

TEST(PlotHeight, TightSqueezes) {
  // Mockup scale: 2 chrome + 2 ifaces + 4 peers * 5 = 24 fixed, 0 left -> 1.
  EXPECT_EQ(plot_height_for(30, 2, 4), 1);
}

TEST(PlotHeight, MidRange) {
  // 2 + 1 + 2*5 = 13 fixed; (40-13)/2 = 13 -> clamp 4.
  EXPECT_EQ(plot_height_for(40, 1, 2), 4);
  // 2 + 1 + 5*5 = 28 fixed; (33-28)/5 = 1.
  EXPECT_EQ(plot_height_for(33, 1, 5), 1);
  // 2 + 1 + 3*5 = 18 fixed; (27-18)/3 = 3.
  EXPECT_EQ(plot_height_for(27, 1, 3), 3);
}

TEST(PlotHeight, TinyTerminalFloor) { EXPECT_EQ(plot_height_for(10, 1, 5), 1); }

TEST(BarRows, EmptyOnBadInput) {
  const auto h = ramp(10);
  EXPECT_TRUE(bar_rows(h, 0, 10, 3).empty());
  EXPECT_TRUE(bar_rows(h, 10, 0, 3).empty());
  EXPECT_TRUE(bar_rows(h, 10, 10, 0).empty());
}

TEST(BarRows, FullColumnIsSolid) {
  std::array<double, 120> h{};
  h[0] = 5.0;
  const auto rows = bar_rows(h, 1, 4, 2);
  ASSERT_EQ(rows.size(), 2u);
  for (const auto& r : rows) {
    ASSERT_EQ(r.size(), 4u);
    for (const char* g : r) EXPECT_STREQ(g, "█");
  }
}
TEST(BarRows, RampRisesLeftToRight) {
  const auto h = ramp(9);
  const auto rows = bar_rows(h, 9, 9, 2);
  ASSERT_EQ(rows.size(), 2u);
  // Leftmost column (value 0) blank, rightmost (max) solid.
  EXPECT_STREQ(rows[0][0], " ");
  EXPECT_STREQ(rows[1][0], " ");
  EXPECT_STREQ(rows[0][8], "█");
  EXPECT_STREQ(rows[1][8], "█");
  // Value 4 of 8 is exactly half: blank on top, solid below.
  EXPECT_STREQ(rows[0][4], " ");
  EXPECT_STREQ(rows[1][4], "█");
  // Value 2 of 8 is a partial block on the bottom row only.
  EXPECT_STREQ(rows[0][2], " ");
  EXPECT_STREQ(rows[1][2], "▄");
}

TEST(PlotMode, ToggleAndName) {
  EXPECT_EQ(plot_name(PlotMode::Line), "line");
  EXPECT_EQ(plot_name(PlotMode::Bar), "bar");
  EXPECT_EQ(next_plot(PlotMode::Line), PlotMode::Bar);
  EXPECT_EQ(next_plot(PlotMode::Bar), PlotMode::Line);
}

TEST(PlotMode, Defaults) {
  const CliOptions o;
  EXPECT_EQ(o.plot_mode, PlotMode::Line);
  EXPECT_EQ(o.theme, 0u);
}

WgFrame synthetic_frame(double t, uint64_t rx_base, int n_peers = 2) {
  WgFrame f;
  f.tstamp = t;
  f.ifaces.push_back(
      WgIface{"wr0", "iface-priv", "iface-pub", 51820});
  const auto now = static_cast<uint64_t>(std::time(nullptr));
  for (int i = 0; i < n_peers; ++i) {
    WgPeer p;
    p.iface = "wr0";
    p.pubkey = "peer" + std::to_string(i);
    p.endpoint = "198.51.100.1:1234";
    p.allowed_ips = "10.255.0.2/32";
    p.handshake = now;
    p.rx = rx_base + static_cast<uint64_t>(i) * 100;
    p.tx = rx_base;
    f.peers.push_back(p);
  }
  return f;
}

TEST(Refresh, BoxesPeersWithHeaderAndFooter) {
  cpptui::App app;
  CliOptions opts;
  RateTracker tracker;
  tracker.ingest(synthetic_frame(1000.0, 100));
  tracker.ingest(synthetic_frame(1001.0, 200));
  Ui ui(app, opts, tracker);
  ui.refresh(synthetic_frame(1001.0, 200));
  const auto root =
      std::dynamic_pointer_cast<cpptui::Container>(ui.root());
  ASSERT_NE(root, nullptr);
  // header, flex body (interface line + two peer boxes), footer.
  const auto& kids = root->get_children();
  ASSERT_EQ(kids.size(), 3u);
  EXPECT_NE(std::dynamic_pointer_cast<cpptui::Label>(kids[0]), nullptr);
  EXPECT_NE(std::dynamic_pointer_cast<cpptui::Label>(kids[2]), nullptr);
  const auto body =
      std::dynamic_pointer_cast<cpptui::Container>(kids[1]);
  ASSERT_NE(body, nullptr);
  const auto& content = body->get_children();
  ASSERT_EQ(content.size(), 3u);
  EXPECT_NE(std::dynamic_pointer_cast<cpptui::Label>(content[0]), nullptr);
  const auto box0 = std::dynamic_pointer_cast<cpptui::Border>(content[1]);
  const auto box1 = std::dynamic_pointer_cast<cpptui::Border>(content[2]);
  ASSERT_NE(box0, nullptr);
  ASSERT_NE(box1, nullptr);
  EXPECT_EQ(box0->fixed_height, box1->fixed_height);
  EXPECT_GE(box0->fixed_height, 6);  // 5 fixed lines + >= 1 plot line
  EXPECT_LE(box0->fixed_height, 9);  // 5 fixed lines + <= 4 plot lines
  // Box content: info, allowed-ips, plots row, stats row.
  const auto inner =
      std::dynamic_pointer_cast<cpptui::Container>(box0->get_children()[0]);
  ASSERT_NE(inner, nullptr);
  ASSERT_EQ(inner->get_children().size(), 4u);
  // Plots row and stats row share geometry: flex / fixed-3 gap / flex,
  // so "tx" starts under the right-hand plot.
  for (const std::size_t row : {2u, 3u}) {
    const auto hrow = std::dynamic_pointer_cast<cpptui::Horizontal>(
        inner->get_children()[row]);
    ASSERT_NE(hrow, nullptr) << "row " << row;
    ASSERT_EQ(hrow->get_children().size(), 3u);
    EXPECT_EQ(hrow->get_children()[1]->fixed_width, 3);
    EXPECT_EQ(hrow->get_children()[0]->fixed_width, 0);
    EXPECT_EQ(hrow->get_children()[2]->fixed_width, 0);
  }
}

TEST(Refresh, StaleFrameShowsErrAndFooter) {
  cpptui::App app;
  CliOptions opts;
  RateTracker tracker;
  Ui ui(app, opts, tracker);
  WgFrame f;
  f.stale = true;
  ui.refresh(f);
  const auto root =
      std::dynamic_pointer_cast<cpptui::Container>(ui.root());
  ASSERT_NE(root, nullptr);
  // header, flex body holding the ERR line, footer.
  const auto& kids = root->get_children();
  ASSERT_EQ(kids.size(), 3u);
  const auto body =
      std::dynamic_pointer_cast<cpptui::Container>(kids[1]);
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->get_children().size(), 1u);
}

}  // namespace

TEST(Refresh, HideInactiveFiltersNeverHandshook) {
  auto frame = synthetic_frame(1001.0, 200);
  frame.peers[1].handshake = 0;
  const auto count_boxes = [&](bool hide) {
    cpptui::App app;
    CliOptions opts;
    opts.hide_inactive = hide;
    RateTracker tracker;
    tracker.ingest(synthetic_frame(1000.0, 100));
    tracker.ingest(synthetic_frame(1001.0, 200));
    Ui ui(app, opts, tracker);
    ui.refresh(frame);
    const auto root =
        std::dynamic_pointer_cast<cpptui::Container>(ui.root());
    const auto body = std::dynamic_pointer_cast<cpptui::Container>(
        root->get_children()[1]);
    return body->get_children().size();
  };
  // iface + two boxes vs one box hidden (inside the flex body).
  EXPECT_EQ(count_boxes(false), 3u);
  EXPECT_EQ(count_boxes(true), 2u);
}

TEST(CompressedHeight, ExactFitMath) {
  // 2 chrome + 2*1 run + 3*2 fixed + 2*h <= 12 -> h = 1.
  EXPECT_EQ(compressed_plot_height(12, 1, 2), 1);
  // (40 - 2 - 2 - 6) / 2 = 15 -> clamp 4.
  EXPECT_EQ(compressed_plot_height(40, 1, 2), 4);
  EXPECT_EQ(compressed_plot_height(8, 1, 5), 1);
  EXPECT_EQ(compressed_plot_height(24, 0, 0), 4);
}

TEST(FitText, FitsUnchangedEllipsizesOverflow) {
  EXPECT_EQ(fit_text("abcdef", 10), "abcdef");
  EXPECT_EQ(fit_text("abcdef", 6), "abcdef");
  EXPECT_EQ(fit_text("abcdef", 5), "abcd…");
  EXPECT_EQ(fit_text("abcdef", 1), "…");
  EXPECT_EQ(fit_text("abcdef", 0), "");
  EXPECT_EQ(fit_text("a…b", 3), "a…b");
  EXPECT_EQ(fit_text("a…b", 2), "a…");
}

TEST(FitRuns, CutsInsideRunKeepingColor) {
  const cpptui::Color red = cpptui::Color::Red();
  const auto out =
      fit_runs({{"ab", red}, {"cdef", std::nullopt}}, 4);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].text, "ab");
  EXPECT_TRUE(out[0].fg.has_value());
  EXPECT_EQ(out[1].text, "c…");
  EXPECT_FALSE(out[1].fg.has_value());
}

TEST(FitRuns, AllFitPassesThrough) {
  const cpptui::Color red = cpptui::Color::Red();
  const auto out =
      fit_runs({{"ab", red}, {"cdef", std::nullopt}}, 10);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].text, "ab");
  EXPECT_EQ(out[1].text, "cdef");
}

TEST(Refresh, CompressedSharesOneBoxWithSeparators) {
  // 10 peers need 2 + 1 + 10*6 = 63 normal lines > fallback 24 rows,
  // so refresh must pick the compressed layout.
  cpptui::App app;
  CliOptions opts;
  RateTracker tracker;
  tracker.ingest(synthetic_frame(1000.0, 100, 10));
  tracker.ingest(synthetic_frame(1001.0, 200, 10));
  Ui ui(app, opts, tracker);
  ui.refresh(synthetic_frame(1001.0, 200, 10));
  const auto root =
      std::dynamic_pointer_cast<cpptui::Container>(ui.root());
  ASSERT_NE(root, nullptr);
  // header, flex body (interface line + shared box), footer.
  const auto& kids = root->get_children();
  ASSERT_EQ(kids.size(), 3u);
  const auto compressed_body =
      std::dynamic_pointer_cast<cpptui::Container>(kids[1]);
  ASSERT_NE(compressed_body, nullptr);
  const auto& content = compressed_body->get_children();
  ASSERT_EQ(content.size(), 2u);
  const auto box = std::dynamic_pointer_cast<CompressedBox>(content[1]);
  ASSERT_NE(box, nullptr);
  // 10 peers x (info + plots + stats) + 9 blank separator rows, which the
  // box overpaints with ├─┤ blended into the outer border.
  const auto inner =
      std::dynamic_pointer_cast<cpptui::Container>(box->get_children()[0]);
  ASSERT_NE(inner, nullptr);
  ASSERT_EQ(inner->get_children().size(), 39u);
  // Merged info line carries the allowed IPs inline.
  const auto info =
      std::dynamic_pointer_cast<cpptui::Label>(inner->get_children()[0]);
  ASSERT_NE(info, nullptr);
  EXPECT_NE(info->get_text().find("(10.255.0.2/32)"), std::string::npos);
}

TEST(Refresh, OverflowKeepsFooterAndInfoRows) {
  // Mirrors the 66x34 field report: 10 peers need more rows than fit, so
  // the flex body clips internally. Footer and every peer info line must
  // keep a full row (separators overpaint only their reserved rows).
  cpptui::App app;
  CliOptions opts;
  RateTracker tracker;
  tracker.ingest(synthetic_frame(1000.0, 100, 10));
  tracker.ingest(synthetic_frame(1001.0, 200, 10));
  Ui ui(app, opts, tracker);
  ui.refresh(synthetic_frame(1001.0, 200, 10));
  const auto root =
      std::dynamic_pointer_cast<cpptui::Container>(ui.root());
  ASSERT_NE(root, nullptr);
  root->x = 0;
  root->y = 0;
  root->width = 66;
  root->height = 34;
  root->layout();
  const auto& kids = root->get_children();
  ASSERT_EQ(kids.size(), 3u);
  const auto footer = kids[2];
  EXPECT_EQ(footer->height, 1);
  EXPECT_EQ(footer->y, 33);
  const auto body =
      std::dynamic_pointer_cast<cpptui::Container>(kids[1]);
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->get_children().size(), 2u);
  const auto box =
      std::dynamic_pointer_cast<CompressedBox>(body->get_children()[1]);
  ASSERT_NE(box, nullptr);
  const auto inner =
      std::dynamic_pointer_cast<cpptui::Container>(box->get_children()[0]);
  ASSERT_NE(inner, nullptr);
  // 29 content rows fit; the 10 tail rows clip to 0 in order (graceful
  // bottom cut). The regression was rows swallowed mid-list.
  ASSERT_EQ(inner->get_children().size(), 39u);
  for (std::size_t i = 0; i < inner->get_children().size(); ++i)
    EXPECT_EQ(inner->get_children()[i]->height, i < 29 ? 1 : 0)
        << "child " << i;
}
