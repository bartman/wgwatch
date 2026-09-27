#include <gtest/gtest.h>

#include <sstream>

#include "../src/config.hpp"

namespace {

CliOptions parse_argv(const std::vector<std::string>& args, CliOptions base) {
  std::vector<char*> argv;
  argv.reserve(args.size());
  for (auto& a : const_cast<std::vector<std::string>&>(args))
    argv.push_back(a.data());
  return parse_cli(static_cast<int>(argv.size()), argv.data(), base);
}

TEST(Config, ExactDefaultFormat) {
  std::ostringstream out;
  wconfig::write_config(out, CliOptions{});
  EXPECT_EQ(out.str(),
            "update = 1\n"
            "sort = mru\n"
            "plot = line\n"
            "theme = catppuccin-mocha\n"
            "inactive = show\n"
            "public_keys = hide\n");
}

TEST(Config, RoundTripKeepsEverything) {
  CliOptions o;
  o.update_sec = 0.5;
  o.sort = SortKey::Rate;
  o.plot_mode = PlotMode::Bar;
  o.theme = 6;
  o.hide_inactive = true;
  o.show_keys = true;
  o.interface = "wg0";  // never persisted
  o.remote = "h";       // never persisted
  std::ostringstream out;
  wconfig::write_config(out, o);
  CliOptions back;
  std::istringstream in(out.str());
  wconfig::read_config(in, back);
  EXPECT_DOUBLE_EQ(back.update_sec, 0.5);
  EXPECT_EQ(back.sort, SortKey::Rate);
  EXPECT_EQ(back.plot_mode, PlotMode::Bar);
  EXPECT_EQ(back.theme, 6u);
  EXPECT_TRUE(back.hide_inactive);
  EXPECT_TRUE(back.show_keys);
  EXPECT_EQ(back.interface, "all");
  EXPECT_FALSE(back.remote.has_value());
}

TEST(Config, BadLinesIgnoredLastValidWins) {
  CliOptions o;
  std::istringstream in(
      "no equals here\n"
      "frobnicate = 1\n"
      "update = 99999\n"
      "sort = wat\n"
      "plot = dots\n"
      "theme = nope\n"
      "inactive = maybe\n"
      "public_keys = sometimes\n"
      "update = 2\n");
  wconfig::read_config(in, o);
  EXPECT_DOUBLE_EQ(o.update_sec, 2.0);
  EXPECT_EQ(o.sort, SortKey::Mru);
  EXPECT_EQ(o.plot_mode, PlotMode::Line);
  EXPECT_EQ(o.theme, 0u);
  EXPECT_FALSE(o.hide_inactive);
  EXPECT_FALSE(o.show_keys);
}

TEST(Config, CliOverridesFile) {
  CliOptions base;
  base.update_sec = 5.0;
  base.theme = 6;
  base.hide_inactive = true;
  const CliOptions o =
      parse_argv({"wgwatch", "--theme", "nord"}, base);
  EXPECT_EQ(o.theme, 2u);  // nord
  EXPECT_DOUBLE_EQ(o.update_sec, 5.0);  // from base, no CLI flag
  EXPECT_TRUE(o.hide_inactive);         // from base, no CLI flag
  const CliOptions o2 =
      parse_argv({"wgwatch", "--inactive", "show"}, base);
  EXPECT_FALSE(o2.hide_inactive);
}

}  // namespace
