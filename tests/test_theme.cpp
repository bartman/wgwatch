#include <gtest/gtest.h>

#include "../src/theme.hpp"

namespace {

TEST(Theme, TwelveBuiltins) { EXPECT_EQ(wtheme::num_themes(), 12u); }

TEST(Theme, DefaultIsCatppuccinMocha) {
  EXPECT_STREQ(wtheme::theme_at(0).name, "catppuccin-mocha");
}

TEST(Theme, NamesUnique) {
  for (std::size_t i = 0; i < wtheme::num_themes(); ++i)
    for (std::size_t j = i + 1; j < wtheme::num_themes(); ++j)
      EXPECT_STRNE(wtheme::theme_at(i).name, wtheme::theme_at(j).name);
}

TEST(Theme, CycleWraps) {
  EXPECT_STREQ(wtheme::theme_at(wtheme::num_themes()).name,
               wtheme::theme_at(0).name);
}

// Spot-checks against https://terminalcolors.com palettes.
TEST(Theme, DraculaColors) {
  const wtheme::Theme& t = wtheme::theme_at(1);
  EXPECT_STREQ(t.name, "dracula");
  EXPECT_STREQ(t.bg, "#282a36");
  EXPECT_STREQ(t.fg, "#f8f8f2");
  EXPECT_STREQ(t.rx, "#50fa7b");
  EXPECT_STREQ(t.tx, "#8be9fd");
  EXPECT_STREQ(t.err, "#ff5555");
}

TEST(Theme, NordColors) {
  const wtheme::Theme& t = wtheme::theme_at(2);
  EXPECT_STREQ(t.bg, "#2e3440");
  EXPECT_STREQ(t.header, "#81a1c1");
}

TEST(Theme, GithubDarkColors) {
  const wtheme::Theme& t = wtheme::theme_at(6);
  EXPECT_STREQ(t.name, "github-dark");
  EXPECT_STREQ(t.bg, "#010409");
  EXPECT_STREQ(t.rx, "#3fb950");
  EXPECT_STREQ(t.tx, "#39c5cf");
}

TEST(Theme, ShadesOfPurpleColors) {
  const wtheme::Theme& t = wtheme::theme_at(7);
  EXPECT_STREQ(t.name, "shades-of-purple");
  EXPECT_STREQ(t.bg, "#1e1e3f");
  EXPECT_STREQ(t.rx, "#3ad900");
  EXPECT_STREQ(t.tx, "#80fcff");
}

TEST(Theme, HexParses) {
  const cpptui::Color c = wtheme::color("#1a1b26");
  EXPECT_EQ(c.r, 0x1a);
  EXPECT_EQ(c.g, 0x1b);
  EXPECT_EQ(c.b, 0x26);
  EXPECT_FALSE(c.is_default);
}

}  // namespace
