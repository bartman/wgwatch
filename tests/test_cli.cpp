#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "../src/cli.hpp"

namespace {

CliOptions run(const std::vector<std::string>& args) {
  std::vector<char*> argv;
  argv.reserve(args.size());
  for (auto& a : const_cast<std::vector<std::string>&>(args))
    argv.push_back(a.data());
  return parse_cli(static_cast<int>(argv.size()), argv.data());
}

}  // namespace

TEST(Cli, Defaults) {
  const CliOptions o = run({"wgwatch"});
  EXPECT_FALSE(o.show_keys);
  EXPECT_DOUBLE_EQ(o.update_sec, 1.0);
  EXPECT_EQ(o.interface, "all");
  EXPECT_FALSE(o.remote.has_value());
}

TEST(Cli, FullOptions) {
  const CliOptions o =
      run({"wgwatch", "-u", "0.5", "-i", "wg0", "-r", "h", "--show-keys"});
  EXPECT_DOUBLE_EQ(o.update_sec, 0.5);
  EXPECT_EQ(o.interface, "wg0");
  ASSERT_TRUE(o.remote.has_value());
  EXPECT_EQ(*o.remote, "h");
  EXPECT_TRUE(o.show_keys);
}

TEST(Cli, BadUpdateRejected) {
  EXPECT_THROW(
      {
        try {
          run({"wgwatch", "-u", "abc"});
        } catch (const std::invalid_argument& e) {
          EXPECT_NE(std::string(e.what()).find("invalid --update"),
                    std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
  EXPECT_THROW(run({"wgwatch", "-u", "0.01"}), std::invalid_argument);
  EXPECT_THROW(run({"wgwatch", "-u", "3601"}), std::invalid_argument);
}

TEST(Cli, BadInterfaceRejected) {
  EXPECT_THROW(
      {
        try {
          run({"wgwatch", "-i", "evil/iface"});
        } catch (const std::invalid_argument& e) {
          EXPECT_NE(std::string(e.what()).find("invalid --interface"),
                    std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(Cli, BadRemoteRejected) {
  EXPECT_THROW(
      {
        try {
          run({"wgwatch", "-r", "evil;rm -rf"});
        } catch (const std::invalid_argument& e) {
          EXPECT_NE(std::string(e.what()).find("invalid --remote"),
                    std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(Cli, MissingRemoteValue) {
  EXPECT_THROW(
      {
        try {
          run({"wgwatch", "-r"});
        } catch (const std::invalid_argument& e) {
          EXPECT_NE(std::string(e.what()).find("missing value for --remote"),
                    std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(Cli, HelpThrows) { EXPECT_THROW(run({"wgwatch", "-h"}), HelpRequested); }

TEST(Cli, SortDefaultMru) {
  EXPECT_EQ(run({"wgwatch"}).sort, SortKey::Mru);
}

TEST(Cli, SortValues) {
  EXPECT_EQ(run({"wgwatch", "-s", "endpoint"}).sort, SortKey::Endpoint);
  EXPECT_EQ(run({"wgwatch", "--sort", "rx-rate"}).sort, SortKey::RxRate);
  EXPECT_EQ(run({"wgwatch", "--sort", "tx-bytes"}).sort, SortKey::TxBytes);
  EXPECT_EQ(run({"wgwatch", "--sort", "bytes"}).sort, SortKey::Bytes);
  EXPECT_EQ(run({"wgwatch", "--sort", "rate"}).sort, SortKey::Rate);
}

TEST(Cli, SortHelpThrows) {
  EXPECT_THROW(run({"wgwatch", "--sort", "help"}), SortHelpRequested);
}

TEST(Cli, SortBadValue) {
  EXPECT_THROW(run({"wgwatch", "--sort", "bogus"}), std::invalid_argument);
}

TEST(Cli, VerboseCounts) {
  EXPECT_EQ(run({"wgwatch"}).verbose, 0);
  EXPECT_EQ(run({"wgwatch", "-v"}).verbose, 1);
  EXPECT_EQ(run({"wgwatch", "-v", "-v"}).verbose, 2);
  EXPECT_EQ(run({"wgwatch", "-vv"}).verbose, 2);
  EXPECT_EQ(run({"wgwatch", "--verbose"}).verbose, 1);
}

TEST(Cli, LogFile) {
  EXPECT_FALSE(run({"wgwatch"}).log_file.has_value());
  const CliOptions o = run({"wgwatch", "--log", "/tmp/wgwatch.log"});
  ASSERT_TRUE(o.log_file.has_value());
  EXPECT_EQ(*o.log_file, "/tmp/wgwatch.log");
  EXPECT_THROW(run({"wgwatch", "--log"}), std::invalid_argument);
}

TEST(Cli, UsageMentionsLogging) {
  EXPECT_NE(usage().find("--verbose"), std::string::npos);
  EXPECT_NE(usage().find("--log"), std::string::npos);
}

TEST(Cli, HideInactive) {
  EXPECT_FALSE(run({"wgwatch"}).hide_inactive);
  EXPECT_TRUE(run({"wgwatch", "--hide-inactive"}).hide_inactive);
}

TEST(Cli, VersionThrowsAndFormats) {
  EXPECT_THROW(run({"wgwatch", "--version"}), VersionRequested);
  EXPECT_EQ(version_string(), "wgwatch " WGWATCH_VERSION);
  EXPECT_NE(version_string().find("wgwatch "), std::string::npos);
}
