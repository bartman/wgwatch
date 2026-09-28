#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

#include "parser.hpp"
#include "rates.hpp"

// Forward declarations keep <ev.h> confined to sampler.cpp.
struct ev_loop;
struct ev_io;
struct ev_async;
struct ev_timer;

// Owns one thread running ev_default_loop + ev_io on the collector fd.
// Timestamp lines (^\d+\.\d+$) delimit frames; each block is parsed,
// ingested, and forwarded via Cb. EOF emits one stale frame and stops.
// A repeating timer emits a stale frame whenever no bytes arrive for
// stall_after_sec (covers silent stalls: sudo prompt, dead loop, …).
class Sampler {
 public:
  using Cb = std::function<void(WgFrame)>;
  using ErrCb = std::function<void(std::string)>;
  Sampler(int fd, int err_fd, std::string filter, double stall_after_sec,
          RateTracker& tracker, Cb cb, ErrCb err_cb);
  ~Sampler();
  Sampler(const Sampler&) = delete;
  Sampler& operator=(const Sampler&) = delete;

  void start();
  void stop();

 private:
  static void io_cb(struct ev_loop* loop, struct ev_io* w, int revents);
  static void err_cb(struct ev_loop* loop, struct ev_io* w, int revents);
  static void wake_cb(struct ev_loop* loop, struct ev_async* w, int revents);
  static void timer_cb(struct ev_loop* loop, struct ev_timer* w, int revents);
  void run();
  void on_data();
  void on_err_data();
  void on_timeout();
  void emit_block();
  void emit_stale();

  int fd_;
  int err_fd_;
  std::string filter_;
  double stall_after_sec_;
  RateTracker* tracker_;
  Cb cb_;
  ErrCb err_cb_;
  std::thread thread_;
  bool running_ = false;
  std::atomic<struct ev_loop*> loop_{nullptr};
  struct ev_io* io_;
  struct ev_io* err_io_;
  struct ev_async* wake_;
  struct ev_timer* timer_;
  std::chrono::steady_clock::time_point last_rx_;
  std::string buf_;
  std::string err_buf_;
  std::string cur_;
  double cur_ts_ = 0.0;
  bool have_block_ = false;
};
