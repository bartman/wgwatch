#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "../src/collector.hpp"

TEST(CollectorCmd, LocalContainsDumpAndSleep) {
  CliOptions o;
  const std::string cmd = build_loop_command(o);
  EXPECT_NE(cmd.find("wg show all dump"), std::string::npos);
  EXPECT_NE(cmd.find("sleep 1.000"), std::string::npos);
  EXPECT_NE(cmd.find("date +%s.%N"), std::string::npos);
  EXPECT_EQ(cmd.find("ssh"), std::string::npos);
}

TEST(CollectorCmd, LocalExitsWhenReaderDrops) {
  CliOptions o;
  const std::string cmd = build_loop_command(o);
  EXPECT_NE(cmd.find("trap 'exit 0' HUP TERM INT PIPE"), std::string::npos);
  EXPECT_NE(cmd.find("date +%s.%N || exit"), std::string::npos);
  EXPECT_NE(cmd.find("|| exit; done"), std::string::npos);
}

TEST(CollectorCmd, CustomIntervalFormatsMillis) {
  CliOptions o;
  o.update_sec = 0.5;
  EXPECT_NE(build_loop_command(o).find("sleep 0.500"), std::string::npos);
}

TEST(CollectorCmd, RemoteBodyHasNoSshWrapper) {
  CliOptions o;
  o.remote = "user@example.com";
  const std::string cmd = build_remote_command(o);
  EXPECT_NE(cmd.find("wg show all dump"), std::string::npos);
  EXPECT_NE(cmd.find("sleep 1.000"), std::string::npos);
  EXPECT_NE(cmd.find("trap 'exit 0' HUP TERM INT PIPE"), std::string::npos);
  EXPECT_EQ(cmd.find("ssh"), std::string::npos);
  EXPECT_EQ(cmd.find("user@example.com"), std::string::npos);
}

TEST(CollectorCmd, SshArgvCarriesFlagsAndCommand) {
  CliOptions o;
  o.remote = "user@example.com";
  const std::string body = build_remote_command(o);
  const std::vector<std::string> argv = build_ssh_argv(o, body);
  ASSERT_EQ(argv.size(), 10u);
  EXPECT_EQ(argv[0], "ssh");
  EXPECT_EQ(argv[1], "-n");
  EXPECT_EQ(argv[2], "-F");
  EXPECT_EQ(argv[3], "/dev/null");
  EXPECT_EQ(argv[5], "BatchMode=yes");
  EXPECT_EQ(argv[7], "ConnectTimeout=10");
  EXPECT_EQ(argv[8], "user@example.com");
  EXPECT_EQ(argv.back(), body);
}

TEST(CollectorCmd, CustomCommandReplacesWg) {
  CliOptions o;
  o.command = "/my/version/of/wg";
  EXPECT_NE(build_loop_command(o).find("/my/version/of/wg show all dump"),
            std::string::npos);
  EXPECT_NE(build_remote_command(o).find("/my/version/of/wg show all dump"),
            std::string::npos);
}
