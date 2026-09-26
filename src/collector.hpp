#pragma once

#include <string>
#include <vector>

#include "cli.hpp"

// Pure command builders (tested) + forked loop owner (not tested).
// No system(): pipe2(O_CLOEXEC|O_NONBLOCK) + fork + dup2 + exec.
// The child gets its own process group so stop() can signal the whole
// tree (shell + sleep/sudo/ssh), plus PR_SET_PDEATHSIG so an untimely
// parent death still takes the collector down. Remote runs ssh directly
// (no local sh wrapper); the remote loop traps HUP/TERM/INT/PIPE and
// exits on any failed timestamp write, so a dropped connection can never
// strand a bash on the target.
std::string build_loop_command(const CliOptions& o);  // local `sh -c` body
std::string build_remote_command(const CliOptions& o);  // remote shell body
std::vector<std::string> build_ssh_argv(const CliOptions& o,
                                        const std::string& remote_cmd);

class Collector {
 public:
  Collector() = default;
  Collector(const Collector&) = delete;
  Collector& operator=(const Collector&) = delete;
  ~Collector() { stop(); }

  void start(const CliOptions& o);
  void stop();
  int fd() const { return fd_; }

 private:
  int fd_ = -1;
  int pid_ = -1;
  bool group_ok_ = false;
};
