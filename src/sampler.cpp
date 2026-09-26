#include "sampler.hpp"

#include <ev.h>
#include <spdlog/spdlog.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {

bool is_ts_line(const std::string& line) {
  if (line.empty()) return false;
  bool dot = false;
  for (const char c : line) {
    if (c == '.') {
      if (dot) return false;
      dot = true;
    } else if (c < '0' || c > '9') {
      return false;
    }
  }
  return dot;
}

}  // namespace

Sampler::Sampler(int fd, std::string filter, double stall_after_sec,
                 RateTracker& tracker, Cb cb)
    : fd_(fd),
      filter_(std::move(filter)),
      stall_after_sec_(stall_after_sec),
      tracker_(&tracker),
      cb_(std::move(cb)) {
  io_ = new ev_io;
  wake_ = new ev_async;
  timer_ = new ev_timer;
  last_rx_ = std::chrono::steady_clock::now();
}

Sampler::~Sampler() {
  stop();
  delete io_;
  delete wake_;
  delete timer_;
}

void Sampler::io_cb(struct ev_loop* /*loop*/, struct ev_io* w, int /*revents*/) {
  static_cast<Sampler*>(w->data)->on_data();
}

void Sampler::wake_cb(struct ev_loop* loop, struct ev_async* /*w*/,
                      int /*revents*/) {
  ev_break(loop, EVBREAK_ALL);
}

void Sampler::timer_cb(struct ev_loop* /*loop*/, struct ev_timer* w,
                       int /*revents*/) {
  static_cast<Sampler*>(w->data)->on_timeout();
}

void Sampler::run() {
  spdlog::debug("sampler: loop starting (fd={}, filter={}, stall={:.1f}s)",
                fd_, filter_, stall_after_sec_);
  struct ev_loop* loop = ev_default_loop(0);
  loop_.store(loop);
  ev_io_init(io_, &Sampler::io_cb, fd_, EV_READ);
  io_->data = this;
  ev_io_start(loop, io_);
  ev_async_init(wake_, &Sampler::wake_cb);
  ev_async_start(loop, wake_);
  ev_timer_init(timer_, &Sampler::timer_cb, stall_after_sec_,
                stall_after_sec_);
  timer_->data = this;
  ev_timer_start(loop, timer_);
  last_rx_ = std::chrono::steady_clock::now();
  ev_run(loop, 0);
  ev_io_stop(loop, io_);
  ev_async_stop(loop, wake_);
  ev_timer_stop(loop, timer_);
  loop_.store(nullptr);
  spdlog::debug("sampler: loop exited");
}

void Sampler::start() {
  if (running_) return;
  running_ = true;
  thread_ = std::thread(&Sampler::run, this);
  spdlog::debug("sampler: thread started");
}

void Sampler::stop() {
  if (!running_) return;
  running_ = false;
  if (struct ev_loop* loop = loop_.load()) ev_async_send(loop, wake_);
  if (thread_.joinable()) thread_.join();
  spdlog::debug("sampler: stopped");
}

void Sampler::emit_block() {
  WgFrame f = parse_frame(cur_, filter_);
  f.tstamp = cur_ts_;
  tracker_->ingest(f);
  spdlog::trace("sampler: emit t={:.3f} peers={}", f.tstamp, f.peers.size());
  cb_(std::move(f));
  have_block_ = false;
}

void Sampler::emit_stale() {
  WgFrame stale;
  stale.stale = true;
  stale.tstamp = cur_ts_;
  cb_(std::move(stale));
}

void Sampler::on_timeout() {
  const auto idle = std::chrono::steady_clock::now() - last_rx_;
  const double idle_s = std::chrono::duration<double>(idle).count();
  if (idle_s < stall_after_sec_) return;
  spdlog::warn("sampler: no collector data for {:.1f}s, marking stale",
               idle_s);
  emit_stale();
}

void Sampler::on_data() {
  char tmp[65536];
  bool eof = false;
  std::size_t got = 0;
  while (true) {
    const ssize_t n = ::read(fd_, tmp, sizeof(tmp));
    if (n > 0) {
      buf_.append(tmp, static_cast<std::size_t>(n));
      got += static_cast<std::size_t>(n);
    } else if (n == 0) {
      eof = true;
      break;
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) break;
      spdlog::warn("wgwatch: collector read error: {}", std::strerror(errno));
      eof = true;
      break;
    }
  }
  if (got > 0) {
    last_rx_ = std::chrono::steady_clock::now();
    spdlog::trace("sampler: read {}B (buffered {}B)", got, buf_.size());
  }
  size_t start = 0;
  while (true) {
    const size_t nl = buf_.find('\n', start);
    if (nl == std::string::npos) break;
    std::string line = buf_.substr(start, nl - start);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (is_ts_line(line)) {
      if (have_block_) emit_block();
      cur_.clear();
      try {
        cur_ts_ = std::stod(line);
      } catch (...) {
      }
      have_block_ = true;
    } else if (have_block_) {
      cur_ += line;
      cur_ += '\n';
    }
    start = nl + 1;
  }
  buf_.erase(0, start);
  if (eof) {
    spdlog::warn("sampler: collector EOF, sending stale and exiting");
    if (have_block_) emit_block();
    emit_stale();
    if (struct ev_loop* loop = loop_.load()) ev_break(loop, EVBREAK_ALL);
  }
}
