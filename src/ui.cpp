#include "ui.hpp"

#include <ctime>
#include <map>
#include <utility>
#include <vector>

#include <sys/ioctl.h>
#include <unistd.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "format.hpp"

namespace {

int term_rows() {
  struct winsize w {};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_row > 0)
    return w.ws_row;
  return 24;
}

std::string hidden_key(const std::string& key, bool show) {
  return show ? key : "{hidden}";
}

}  // namespace

PlotWidget::PlotWidget(std::array<double, 120> hist, std::size_t n,
                       PlotMode mode, cpptui::Color fg)
    : hist_(hist), n_(n), mode_(mode), fg_(fg) {}

void PlotWidget::render(cpptui::Buffer& buffer) {
  if (width <= 0 || height <= 0) return;
  // Solid black rectangle; the graph draws over it. Fill runs before the
  // n_ early-out so the rect is black even with no samples yet.
  cpptui::Cell bgc;
  bgc.content = " ";
  bgc.bg_color = cpptui::Color::Black();
  for (int cy = 0; cy < height; ++cy)
    for (int cx = 0; cx < width; ++cx) buffer.set(x + cx, y + cy, bgc);
  if (n_ < 1) return;
  if (mode_ == PlotMode::Bar) {
    const auto rows = bar_rows(hist_, n_, width, height);
    for (int r = 0; r < height && r < static_cast<int>(rows.size()); ++r) {
      for (int c = 0; c < width && c < static_cast<int>(rows[r].size()); ++c) {
        if (rows[r][static_cast<std::size_t>(c)][0] == ' ') continue;
        cpptui::Cell cell;
        cell.content = rows[r][static_cast<std::size_t>(c)];
        cell.fg_color = fg_;
        cell.bg_color = cpptui::Color::Black();
        buffer.set(x + c, y + r, cell);
      }
    }
    return;
  }
  if (n_ < 2) return;
  double maxv = 0.0;
  for (std::size_t i = 0; i < n_; ++i) maxv = std::max(maxv, hist_[i]);
  if (maxv <= 0.0) maxv = 1.0;
  cpptui::BrailleCanvas bc(width, height);
  const int vw = width * 2;
  const int vh = height * 4;
  const auto pt = [&](std::size_t i) {
    const int vx = (n_ <= 1) ? 0 : static_cast<int>(i * (vw - 1) / (n_ - 1));
    const int vy = vh - 1 - static_cast<int>(hist_[i] / maxv * (vh - 1));
    return std::make_pair(vx, vy);
  };
  for (std::size_t i = 1; i < n_; ++i) {
    const auto [x0, y0] = pt(i - 1);
    const auto [x1, y1] = pt(i);
    bc.draw_line(x0, y0, x1, y1);
  }
  for (int cy = 0; cy < height; ++cy) {
    for (int cx = 0; cx < width; ++cx) {
      const std::string ch = bc.get_char(cx, cy);
      if (!ch.empty() && ch != " ") {
        cpptui::Cell c;
        c.content = ch;
        c.fg_color = fg_;
        c.bg_color = cpptui::Color::Black();
        buffer.set(x + cx, y + cy, c);
      }
    }
  }
}

Ui::Ui(cpptui::App& app, CliOptions& opts, RateTracker& tracker)
    : app_(&app), opts_(&opts), tracker_(&tracker) {
  root_ = std::make_shared<cpptui::Vertical>();
  header_ = std::make_shared<cpptui::Label>(cpptui::StyledText("wgwatch"));
  footer_ = std::make_shared<cpptui::Label>(cpptui::StyledText(""));
  root_->add(header_);
  root_->add(footer_);
  app_->register_exit_key('q');
  app_->register_key('k', [this] {
    opts_->show_keys = !opts_->show_keys;
    refresh(last_);
  });
  app_->register_key('s', [this] {
    opts_->sort = next_sort(opts_->sort);
    refresh(last_);
  });
  app_->register_key('p', [this] {
    opts_->plot_mode = next_plot(opts_->plot_mode);
    refresh(last_);
  });
  app_->register_key('t', [this] {
    opts_->theme = (opts_->theme + 1) % wtheme::num_themes();
    refresh(last_);
  });
}

void Ui::refresh(const WgFrame& frame) {
  last_ = frame;
  spdlog::trace("ui: refresh peers={} sort={} plot={} theme={} stale={}",
                frame.peers.size(), sort_name(opts_->sort),
                plot_name(opts_->plot_mode),
                wtheme::theme_at(opts_->theme).name, frame.stale);
  const wtheme::Theme& th = wtheme::theme_at(opts_->theme);
  const cpptui::Color fg = wtheme::color(th.fg);
  const cpptui::Color dim = wtheme::color(th.muted);
  const cpptui::Color hdc = wtheme::color(th.header);
  const cpptui::Color rxc = wtheme::color(th.rx);
  const cpptui::Color txc = wtheme::color(th.tx);
  const cpptui::Color okc = wtheme::color(th.ok);
  const cpptui::Color wac = wtheme::color(th.warn);
  const cpptui::Color erc = wtheme::color(th.err);

  root_->clear_children();
  header_->set_text(cpptui::StyledText(fmt::format(
      "wgwatch | {} | {} peers | last {} shown",
      opts_->remote ? *opts_->remote : std::string("local"),
      frame.peers.size(), wfmt::fmt_span(tracker_->history_span()))));
  header_->fg_color = hdc;
  root_->add(header_);
  const auto add_footer = [&] {
    footer_->set_text(cpptui::StyledText(fmt::format(
        "q quit, s sort ({}), p plot ({}), t theme ({}), k keys ({})",
        sort_name(opts_->sort), plot_name(opts_->plot_mode), th.name,
        opts_->show_keys ? "shown" : "hidden")));
    footer_->fg_color = dim;
    root_->add(footer_);
  };
  if (frame.stale) {
    root_->add(std::make_shared<cpptui::Label>(
        cpptui::StyledText().colored(
            "ERR: no data from collector for 3s+ (wg missing? privileges?)",
            erc)));
    add_footer();
    return;
  }
  const auto states = tracker_->peers();
  const std::vector<const WgPeer*> order =
      sort_peers(frame, states, opts_->sort);
  std::map<std::string, const WgIface*> ifaces;
  for (const WgIface& fi : frame.ifaces) ifaces[fi.name] = &fi;
  // Header lines actually emitted: one per interface transition in display
  // order (non-native sorts can repeat an interface).
  int n_ifaces = 0;
  for (std::size_t i = 0; i < order.size(); ++i) {
    if (i == 0 || order[i]->iface != order[i - 1]->iface) ++n_ifaces;
  }
  const int h = plot_height_for(term_rows(), n_ifaces,
                                static_cast<int>(order.size()));
  std::string cur_iface;
  bool first_iface = true;
  for (const WgPeer* pp : order) {
    const WgPeer& p = *pp;
    if (first_iface || p.iface != cur_iface) {
      cur_iface = p.iface;
      first_iface = false;
      const auto it = ifaces.find(p.iface);
      const std::string port =
          (it != ifaces.end() && it->second->port != 0)
              ? std::to_string(it->second->port)
              : "off";
      const std::string ikey =
          (it != ifaces.end())
              ? hidden_key(it->second->pubkey, opts_->show_keys)
              : "{hidden}";
      root_->add(std::make_shared<cpptui::Label>(
          cpptui::StyledText(
              fmt::format("{} port={} key={}", p.iface, port, ikey)),
          hdc));
    }
    const auto it = states.find(p.iface + "|" + p.pubkey);
    const double rxr = it != states.end() ? it->second.rx_rate : 0.0;
    const double txr = it != states.end() ? it->second.tx_rate : 0.0;
    cpptui::Color hc = erc;
    std::string ago = "never";
    if (p.handshake != 0) {
      const double age = std::difftime(
          std::time(nullptr), static_cast<time_t>(p.handshake));
      ago = wfmt::fmt_ago(age);
      if (age < 180.0)
        hc = okc;
      else if (age < 600.0)
        hc = wac;
    }
    auto box =
        std::make_shared<cpptui::Border>(cpptui::BorderStyle::Single, dim);
    box->fixed_height = 5 + h;
    auto inner = std::make_shared<cpptui::Vertical>();
    cpptui::StyledText info;
    info.add(fmt::format("{}   handshake:", p.endpoint.empty() ? "(none)"
                                                              : p.endpoint));
    info.colored(ago, hc);
    info.add("   key:");
    info.colored(hidden_key(p.pubkey, opts_->show_keys), fg);
    inner->add(std::make_shared<cpptui::Label>(info, fg));
    inner->add(std::make_shared<cpptui::Label>(
        cpptui::StyledText(fmt::format("({})", p.allowed_ips)), dim));
    auto plots = std::make_shared<cpptui::Horizontal>();
    plots->fixed_height = h;
    auto rx = std::make_shared<PlotWidget>(
        it != states.end() ? it->second.rx_hist : std::array<double, 120>{},
        it != states.end() ? it->second.n : 0, opts_->plot_mode, rxc);
    rx->fixed_height = h;
    auto gap = std::make_shared<cpptui::Label>(cpptui::StyledText("   "));
    gap->fixed_width = 3;
    auto tx = std::make_shared<PlotWidget>(
        it != states.end() ? it->second.tx_hist : std::array<double, 120>{},
        it != states.end() ? it->second.n : 0, opts_->plot_mode, txc);
    tx->fixed_height = h;
    plots->add(rx);
    plots->add(gap);
    plots->add(tx);
    inner->add(plots);
    // Stats row mirrors the plots row geometry (flex / 3-gap / flex) so
    // "tx" starts exactly under the right-hand plot.
    auto stats_row = std::make_shared<cpptui::Horizontal>();
    stats_row->fixed_height = 1;
    cpptui::StyledText rx_stats;
    rx_stats.add(" rx ");
    rx_stats.colored(wfmt::fmt_bytes(p.rx), rxc);
    rx_stats.add(fmt::format(" ({})", wfmt::fmt_rate(rxr)));
    stats_row->add(std::make_shared<cpptui::Label>(rx_stats, fg));
    auto stats_gap = std::make_shared<cpptui::Label>(cpptui::StyledText("   "));
    stats_gap->fixed_width = 3;
    stats_row->add(stats_gap);
    cpptui::StyledText tx_stats;
    tx_stats.add(" tx ");
    tx_stats.colored(wfmt::fmt_bytes(p.tx), txc);
    tx_stats.add(fmt::format(" ({})", wfmt::fmt_rate(txr)));
    stats_row->add(std::make_shared<cpptui::Label>(tx_stats, fg));
    inner->add(stats_row);
    box->add(inner);
    root_->add(box);
  }
  add_footer();
}
