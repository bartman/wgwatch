#include <gtest/gtest.h>

#include "../src/format.hpp"

TEST(Format, Bytes) {
  EXPECT_EQ(wfmt::fmt_bytes(0), "0.00 B");
  EXPECT_EQ(wfmt::fmt_bytes(1023), "1023.00 B");
  EXPECT_EQ(wfmt::fmt_bytes(1024), "1.00 KB");
  EXPECT_EQ(wfmt::fmt_bytes(1536), "1.50 KB");
  EXPECT_EQ(wfmt::fmt_bytes(1024ull * 1024 * 3), "3.00 MB");
}

TEST(Format, RateSuffix) {
  EXPECT_EQ(wfmt::fmt_rate(100.0), "100.00 B/s");
  EXPECT_EQ(wfmt::fmt_rate(2048.0), "2.00 KB/s");
}

TEST(Format, Ago) {
  EXPECT_EQ(wfmt::fmt_ago(-1.0), "never");
  EXPECT_EQ(wfmt::fmt_ago(5.0), "5 seconds ago");
  EXPECT_EQ(wfmt::fmt_ago(120.0), "2 minutes ago");
  EXPECT_EQ(wfmt::fmt_ago(7200.0), "2 hours ago");
}

TEST(Format, Span) {
  EXPECT_EQ(wfmt::fmt_span(0.0), "0 seconds");
  EXPECT_EQ(wfmt::fmt_span(1.0), "1 second");
  EXPECT_EQ(wfmt::fmt_span(1.4), "1 second");
  EXPECT_EQ(wfmt::fmt_span(90.0), "90 seconds");
  EXPECT_EQ(wfmt::fmt_span(119.0), "119 seconds");
}
