#include "collector.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <stdexcept>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "priv.hpp"

namespace {

// Every loop iteration writes a timestamp, so a failed write (or HUP/TERM)
// means the reader is gone: exit at once instead of spinning forever.
constexpr const char* kTrap = "trap 'exit 0' HUP TERM INT PIPE; ";

}  // namespace

std::string build_loop_command(const CliOptions& o) {
  const std::string sec = fmt::format("{:.3f}", o.update_sec);
  const std::string detect = priv::sudo_detect_snippet();
  if (priv::is_root())
    return fmt::format(
        "{}while true; do date +%s.%N || exit; {} show all dump; sleep {} || "
        "exit; done",
        kTrap, o.command, sec);
  return fmt::format(
      "{}PRIV={}; while true; do date +%s.%N || exit; $PRIV {} show all dump; "
      "sleep {} || exit; done",
      kTrap, detect, o.command, sec);
}

std::string build_remote_command(const CliOptions& o) {
  const std::string sec = fmt::format("{:.3f}", o.update_sec);
  const std::string detect = priv::sudo_detect_snippet();
  // Passed to ssh as a single argv (no local shell quoting involved); the
  // remote sshd runs it via the remote shell.
  return fmt::format(
      "{}SUDO={}; while true; do date +%s.%N || exit; $SUDO {} show all dump; "
      "sleep {} || exit; done",
      kTrap, detect, o.command, sec);
}

std::vector<std::string> build_ssh_argv(const CliOptions& o,
                                        const std::string& remote_cmd) {
  // -n: stdin from /dev/null. The remote loop never reads stdin, and
  // without it ssh (background group via setpgid) risks SIGTTIN on the
  // shared tty, freezing the whole pipeline while everything looks alive.
  return {"ssh", "-n", "-F", "/dev/null", "-o", "BatchMode=yes", "-o",
          "ConnectTimeout=10", *o.remote, remote_cmd};
}

void Collector::start(const CliOptions& o) {
  if (o.remote)
    spdlog::debug("collector: starting ssh loop for {}", *o.remote);
  else
    spdlog::debug("collector: starting local loop: {}",
                  build_loop_command(o));
  int fds[2] = {-1, -1};
  if (::pipe2(fds, O_CLOEXEC | O_NONBLOCK) != 0)
    throw std::runtime_error(std::string("pipe2 failed: ") +
                             std::strerror(errno));
  const pid_t pid = ::fork();
  if (pid < 0) {
    const int e = errno;
    ::close(fds[0]);
    ::close(fds[1]);
    throw std::runtime_error(std::string("fork failed: ") +
                             std::strerror(e));
  }
  if (pid == 0) {
    ::setpgid(0, 0);
    // Die with the parent even if nobody calls stop() (kill -9, SIGHUP, …).
    ::prctl(PR_SET_PDEATHSIG, SIGTERM);
    if (::getppid() == 1) ::_exit(127);  // already orphaned
    ::dup2(fds[1], STDOUT_FILENO);
    ::close(fds[0]);
    ::close(fds[1]);
    if (o.remote) {
      const std::string cmd = build_remote_command(o);
      const std::vector<std::string> args = build_ssh_argv(o, cmd);
      std::vector<char*> argv;
      argv.reserve(args.size() + 1);
      for (auto& a : const_cast<std::vector<std::string>&>(args))
        argv.push_back(a.data());
      argv.push_back(nullptr);
      ::execvp(argv[0], argv.data());
    } else {
      const std::string cmd = build_loop_command(o);
      ::execl("/bin/sh", "sh", "-c", cmd.c_str(),
              static_cast<char*>(nullptr));
    }
    ::_exit(127);
  }
  ::close(fds[1]);
  // Join (or confirm) the child's group so stop() can signal the whole
  // tree. EACCES means the child already exec'd after setting its own
  // group, which is the outcome we want anyway.
  group_ok_ = (::setpgid(pid, pid) == 0 || errno == EACCES);
  fd_ = fds[0];
  pid_ = static_cast<int>(pid);
  spdlog::debug("collector: started pid={} fd={} group_kill={}", pid_, fd_,
               group_ok_);
}

void Collector::stop() {
  if (pid_ != -1)
    spdlog::debug("collector: stopping pid={}", pid_);
  if (pid_ != -1) {
    if (group_ok_)
      ::killpg(pid_, SIGTERM);
    else
      ::kill(pid_, SIGTERM);
    int status = 0;
    bool reaped = false;
    for (int i = 0; i < 5; ++i) {
      const pid_t r = ::waitpid(pid_, &status, WNOHANG);
      if (r == pid_ || (r == -1 && errno == ECHILD)) {
        reaped = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!reaped) {
      if (group_ok_)
        ::killpg(pid_, SIGKILL);
      else
        ::kill(pid_, SIGKILL);
      ::waitpid(pid_, &status, 0);
    }
    pid_ = -1;
  }
  if (fd_ != -1) {
    ::close(fd_);
    fd_ = -1;
  }
}
