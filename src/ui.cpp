#include "ui.hpp"

#include <ctime>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "format.hpp"

BrailleGraph::BrailleGraph(std::array<double, 120> hist, std::size_t n,
                           bool is_rx)
    : hist_(hist), n_(n), is_rx_(is_rx) {
  fixed_height = 4;
}

void BrailleGraph::render(cpptui::Buffer& buffer) {
  if (n_ < 2 || width <= 0 || height <= 0) return;
  double maxv = 0.0;
  for (std::size_t i = 0; i < n_; ++i) maxv = std::max(maxv, hist_[i]);
  if (maxv <= 0.0) maxv = 1.0;
  cpptui::BrailleCanvas bc(width, height);
  const int vw = width * 2;
  const int vh = height * 4;
  const auto pt = [&](std::size_t i) {
    const int vx =
        (n_ <= 1) ? 0 : static_cast<int>(i * (vw - 1) / (n_ - 1));
    const int vy = vh - 1 - static_cast<int>(hist_[i] / maxv * (vh - 1));
    return std::make_pair(vx, vy);
  };
  for (std::size_t i = 1; i < n_; ++i) {
    const auto [x0, y0] = pt(i - 1);
    const auto [x1, y1] = pt(i);
    bc.draw_line(x0, y0, x1, y1);
  }
  const cpptui::Color fg =
      is_rx_ ? cpptui::Color::Green() : cpptui::Color::Cyan();
  for (int cy = 0; cy < height; ++cy) {
    for (int cx = 0; cx < width; ++cx) {
      const std::string ch = bc.get_char(cx, cy);
      if (!ch.empty() && ch != " ") {
        cpptui::Cell c;
        c.content = ch;
        c.fg_color = fg;
        buffer.set(x + cx, y + cy, c);
      }
    }
  }
}

Ui::Ui(cpptui::App& app, CliOptions& opts, RateTracker& tracker)
    : app_(&app), opts_(&opts), tracker_(&tracker) {
  root_ = std::make_shared<cpptui::Vertical>();
  header_ = std::make_shared<cpptui::Label>(cpptui::StyledText("wgwatch"));
  root_->add(header_);
  app_->register_exit_key('q');
  app_->register_key('k', [this] {
    opts_->show_keys = !opts_->show_keys;
    refresh(last_);
  });
  app_->register_key('s', [this] {
    opts_->sort = next_sort(opts_->sort);
    refresh(last_);
  });
}

void Ui::refresh(const WgFrame& frame) {
  last_ = frame;
  spdlog::trace("ui: refresh peers={} sort={} stale={}", frame.peers.size(),
                sort_name(opts_->sort), frame.stale);
  root_->clear_children();
  header_->set_text(cpptui::StyledText(fmt::format(
      "wgwatch | {} | {} peers | last {} shown | q quit, s sort ({}), k keys "
      "({})",
      opts_->remote ? *opts_->remote : std::string("local"),
      frame.peers.size(), wfmt::fmt_span(tracker_->history_span()),
      sort_name(opts_->sort), opts_->show_keys ? "shown" : "hidden")));
  root_->add(header_);
  if (frame.stale) {
    root_->add(std::make_shared<cpptui::Label>(
        cpptui::StyledText().colored(
            "ERR: no data from collector for 3s+ (wg missing? privileges?)",
            cpptui::Color::Red())));
    return;
  }
  const auto states = tracker_->peers();
  const std::vector<const WgPeer*> order =
      sort_peers(frame, states, opts_->sort);
  std::string cur_iface;
  for (const WgPeer* pp : order) {
    const WgPeer& p = *pp;
    if (p.iface != cur_iface) {
      cur_iface = p.iface;
      root_->add(std::make_shared<cpptui::Label>(
          cpptui::StyledText(fmt::format("=== {} ===", p.iface))));
    }
    const auto it = states.find(p.iface + "|" + p.pubkey);
    const double rxr = it != states.end() ? it->second.rx_rate : 0.0;
    const double txr = it != states.end() ? it->second.tx_rate : 0.0;
    cpptui::Color hc = cpptui::Color::Red();
    std::string ago = "never";
    if (p.handshake != 0) {
      const double age = std::difftime(
          std::time(nullptr), static_cast<std::time_t>(p.handshake));
      ago = wfmt::fmt_ago(age);
      if (age < 180.0)
        hc = cpptui::Color::Green();
      else if (age < 600.0)
        hc = cpptui::Color::Yellow();
    }
    auto peer = std::make_shared<cpptui::Vertical>();
    cpptui::StyledText info;
    info.add(fmt::format("|-- {} | {} | rx {} ({}) tx {} ({}) | hs ",
                         p.endpoint.empty() ? "(none)" : p.endpoint,
                         p.allowed_ips, wfmt::fmt_bytes(p.rx),
                         wfmt::fmt_rate(rxr), wfmt::fmt_bytes(p.tx),
                         wfmt::fmt_rate(txr)));
    info.colored(ago, hc);
    info.add(fmt::format(" | {}", opts_->show_keys ? p.pubkey : "[hidden]"));
    peer->add(std::make_shared<cpptui::Label>(info));
    if (it != states.end()) {
      peer->add(std::make_shared<BrailleGraph>(it->second.rx_hist,
                                               it->second.n, true));
      peer->add(std::make_shared<BrailleGraph>(it->second.tx_hist,
                                               it->second.n, false));
    }
    root_->add(peer);
  }
}
