#include "ui.hpp"

#include <algorithm>
#include <ctime>
#include <map>
#include <utility>
#include <vector>

#include <sys/ioctl.h>
#include <unistd.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "config.hpp"
#include "format.hpp"

namespace {

// {cols, rows}; 80x24 fallback when stdout is not a terminal.
std::pair<int, int> term_size() {
  struct winsize w {};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_row > 0 &&
      w.ws_col > 0)
    return {w.ws_col, w.ws_row};
  return {80, 24};
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
  root_ = std::make_shared<cpptui::Stack>();
  content_ = std::make_shared<cpptui::Vertical>();
  err_overlay_ = std::make_shared<BottomPin>();
  err_overlay_->visible = false;
  err_overlay_->focusable = false;
  root_->add(content_);
  root_->add(err_overlay_);
  header_ = std::make_shared<cpptui::Label>(cpptui::StyledText("wgwatch"));
  footer_ = std::make_shared<cpptui::Label>(cpptui::StyledText(""));
  content_->add(header_);
  content_->add(footer_);
  app_->register_exit_key('q');
  app_->register_key('k', [this] {
    opts_->show_keys = !opts_->show_keys;
    refresh(last_);
    wconfig::save_config(*opts_);
  });
  app_->register_key('s', [this] {
    opts_->sort = next_sort(opts_->sort);
    refresh(last_);
    wconfig::save_config(*opts_);
  });
  app_->register_key('i', [this] {
    opts_->hide_inactive = !opts_->hide_inactive;
    refresh(last_);
    wconfig::save_config(*opts_);
  });
  app_->register_key('p', [this] {
    opts_->plot_mode = next_plot(opts_->plot_mode);
    refresh(last_);
    wconfig::save_config(*opts_);
  });
  app_->register_key('t', [this] {
    opts_->theme = (opts_->theme + 1) % wtheme::num_themes();
    refresh(last_);
    wconfig::save_config(*opts_);
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

  const auto [term_cols, term_rows] = term_size();
  // Stderr overlay: prune expired lines, then (re)build the red box pinned
  // above the footer. Hidden entirely when no live lines remain.
  prune_errors(errors_, std::chrono::steady_clock::now());
  err_overlay_->clear_children();
  if (errors_.empty()) {
    err_overlay_->visible = false;
  } else {
    auto rows = error_box_rows(errors_, term_cols - 2);
    const int max_rows = term_rows - 1 - 2;
    if (max_rows <= 0) {
      err_overlay_->visible = false;
    } else {
      if (static_cast<int>(rows.size()) > max_rows)
        rows.erase(rows.begin(), rows.end() - max_rows);
      const cpptui::Color red = cpptui::Color::Red();
      auto box =
          std::make_shared<cpptui::Border>(cpptui::BorderStyle::Single, red);
      box->focusable = false;
      box->fixed_height = static_cast<int>(rows.size()) + 2;
      auto err_body = std::make_shared<cpptui::Vertical>();
      err_body->focusable = false;
      for (auto& r : rows) {
        auto lab = std::make_shared<cpptui::Label>(
            cpptui::StyledText().colored(r, red), red);
        lab->focusable = false;
        lab->selectable = false;
        err_body->add(lab);
      }
      box->add(err_body);
      err_overlay_->add(box);
      err_overlay_->visible = true;
    }
  }
  content_->clear_children();
  header_->set_text(to_styled(fit_runs(
      {{fmt::format("wgwatch | {} | {} peers | last {} shown",
                     opts_->remote ? *opts_->remote : std::string("local"),
                     frame.peers.size(),
                     wfmt::fmt_span(tracker_->history_span())),
        hdc}},
      term_cols)));
  header_->fg_color = hdc;
  content_->add(header_);
  // Flex body between the fixed header/footer: on overflow the body clips
  // internally, so the footer (key hints) is never pushed off-screen.
  auto body = std::make_shared<cpptui::Vertical>();
  content_->add(body);
  const auto add_footer = [&] {
    footer_->set_text(to_styled(fit_runs(
        {{fmt::format(
              "q quit, s sort ({}), p plot ({}), t theme ({}), i inactive "
              "({}), k keys ({})",
              sort_name(opts_->sort), plot_name(opts_->plot_mode), th.name,
              opts_->hide_inactive ? "hidden" : "shown",
              opts_->show_keys ? "shown" : "hidden"),
          dim}},
        term_cols)));
    footer_->fg_color = dim;
    content_->add(footer_);
  };
  if (frame.stale) {
    body->add(std::make_shared<cpptui::Label>(
        cpptui::StyledText().colored(
            "ERR: no data from collector for 3s+ (wg missing? privileges?)",
            erc)));
    add_footer();
    return;
  }
  const auto states = tracker_->peers();
  std::vector<const WgPeer*> order =
      sort_peers(frame, states, opts_->sort);
  // Inactive = never handshook. Filter before layout so hidden peers take
  // no space and emit no interface header.
  if (opts_->hide_inactive)
    order.erase(std::remove_if(order.begin(), order.end(),
                               [](const WgPeer* p) {
                                 return p->handshake == 0;
                               }),
                order.end());
  std::map<std::string, const WgIface*> ifaces;
  for (const WgIface& fi : frame.ifaces) ifaces[fi.name] = &fi;
  // Consecutive same-interface runs: one header line each, and one shared
  // box each in compressed mode (non-native sorts can repeat an iface).
  std::vector<std::pair<std::size_t, std::size_t>> runs;
  for (std::size_t i = 0; i < order.size();) {
    std::size_t j = i + 1;
    while (j < order.size() && order[j]->iface == order[i]->iface) ++j;
    runs.emplace_back(i, j);
    i = j;
  }
  const int n_peers = static_cast<int>(order.size());
  const int n_runs = static_cast<int>(runs.size());
  const int h_normal = plot_height_for(term_rows, n_runs, n_peers);
  // Small screen, many peers: compressed shares box borders between peers
  // (1 line, not 2) and merges allowed-IPs into the info line.
  const bool compressed =
      n_peers > 0 && term_rows < 2 + n_runs + n_peers * (5 + h_normal);
  const int h = compressed
                    ? compressed_plot_height(term_rows, n_runs, n_peers)
                    : h_normal;
  // Pre-truncation widths: box content is term_cols - 2, and each stats
  // cell mirrors its plot's flex share (remainder goes left, as flex does).
  const int inner_w = term_cols - 2;
  const int avail = inner_w - 3;
  const int rx_w = avail / 2 + avail % 2;
  const int tx_w = avail / 2;
  const auto fit_label = [&](std::vector<TextRun> line, int w,
                             cpptui::Color c) {
    return std::make_shared<cpptui::Label>(
        to_styled(fit_runs(std::move(line), w)), c);
  };
  const auto handshake = [&](const WgPeer& p) {
    if (p.handshake == 0) return std::make_pair(std::string("never"), erc);
    const double age = std::difftime(
        std::time(nullptr), static_cast<time_t>(p.handshake));
    cpptui::Color hc = erc;
    if (age < 180.0)
      hc = okc;
    else if (age < 600.0)
      hc = wac;
    return std::make_pair(wfmt::fmt_ago(age), hc);
  };
  const auto iface_label = [&](const std::string& name) {
    const auto it = ifaces.find(name);
    const std::string port =
        (it != ifaces.end() && it->second->port != 0)
            ? std::to_string(it->second->port)
            : "off";
    const std::string ikey =
        (it != ifaces.end())
            ? hidden_key(it->second->pubkey, opts_->show_keys)
            : "{hidden}";
    return fit_label(
        {{fmt::format("{} port={} key={}", name, port, ikey), hdc}},
        term_cols, hdc);
  };
  const auto peer_state = [&](const WgPeer& p) -> const PeerState* {
    const auto it = states.find(p.iface + "|" + p.pubkey);
    return it != states.end() ? &it->second : nullptr;
  };
  const auto plots_row = [&](const PeerState* st) {
    auto plots = std::make_shared<cpptui::Horizontal>();
    plots->fixed_height = h;
    auto rx = std::make_shared<PlotWidget>(
        st ? st->rx_hist : std::array<double, 120>{}, st ? st->n : 0,
        opts_->plot_mode, rxc);
    rx->fixed_height = h;
    auto gap = std::make_shared<cpptui::Label>(cpptui::StyledText("   "));
    gap->fixed_width = 3;
    auto tx = std::make_shared<PlotWidget>(
        st ? st->tx_hist : std::array<double, 120>{}, st ? st->n : 0,
        opts_->plot_mode, txc);
    tx->fixed_height = h;
    plots->add(rx);
    plots->add(gap);
    plots->add(tx);
    return plots;
  };
  const auto stats_row = [&](const WgPeer& p, const PeerState* st) {
    const WindowStats rxw =
        st ? window_stats(st->rx_hist, st->n) : WindowStats{};
    const WindowStats txw =
        st ? window_stats(st->tx_hist, st->n) : WindowStats{};
    auto row = std::make_shared<cpptui::Horizontal>();
    row->fixed_height = 1;
    row->add(fit_label({{" rx ", std::nullopt},
                        {wfmt::fmt_bytes(p.rx), rxc},
                        {fmt::format(", avg {}, peak {}",
                                     wfmt::fmt_rate(rxw.avg),
                                     wfmt::fmt_rate(rxw.peak)),
                         std::nullopt}},
                       rx_w, fg));
    auto gap = std::make_shared<cpptui::Label>(cpptui::StyledText("   "));
    gap->fixed_width = 3;
    row->add(gap);
    row->add(fit_label({{" tx ", std::nullopt},
                        {wfmt::fmt_bytes(p.tx), txc},
                        {fmt::format(", avg {}, peak {}",
                                     wfmt::fmt_rate(txw.avg),
                                     wfmt::fmt_rate(txw.peak)),
                         std::nullopt}},
                       tx_w, fg));
    return row;
  };
  const auto info_runs = [&](const WgPeer& p, bool merged) {
    const auto [ago, hc] = handshake(p);
    std::vector<TextRun> line = {
        {fmt::format("{}   handshake:",
                     p.endpoint.empty() ? "(none)" : p.endpoint),
         std::nullopt},
        {ago, hc},
        {"   key:", std::nullopt},
        {hidden_key(p.pubkey, opts_->show_keys), fg},
    };
    if (merged) {
      line.push_back({"  (", std::nullopt});
      line.push_back({p.allowed_ips, dim});
      line.push_back({")", std::nullopt});
    }
    return line;
  };
  if (!compressed) {
    for (const auto& [begin, end] : runs) {
      body->add(iface_label(order[begin]->iface));
      for (std::size_t i = begin; i < end; ++i) {
        const WgPeer& p = *order[i];
        const PeerState* st = peer_state(p);
        auto box =
            std::make_shared<cpptui::Border>(cpptui::BorderStyle::Single, dim);
        box->fixed_height = 5 + h;
        auto inner = std::make_shared<cpptui::Vertical>();
        inner->add(fit_label(info_runs(p, false), inner_w, fg));
        inner->add(fit_label({{ "(" + p.allowed_ips + ")", dim }}, inner_w,
                             dim));
        inner->add(plots_row(st));
        inner->add(stats_row(p, st));
        box->add(inner);
        body->add(box);
      }
    }
  } else {
    for (const auto& [begin, end] : runs) {
      body->add(iface_label(order[begin]->iface));
      const int np = static_cast<int>(end - begin);
      auto box = std::make_shared<CompressedBox>(np, h, dim);
      auto inner = std::make_shared<cpptui::Vertical>();
      for (std::size_t i = begin; i < end; ++i) {
        const WgPeer& p = *order[i];
        const PeerState* st = peer_state(p);
        inner->add(fit_label(info_runs(p, true), inner_w, fg));
        inner->add(plots_row(st));
        inner->add(stats_row(p, st));
        // Blank row reserving the separator line: CompressedBox overpaints
        // it with ├─┤ blended into the outer border.
        if (i + 1 < end)
          inner->add(std::make_shared<cpptui::Label>(cpptui::StyledText("")));
      }
      box->add(inner);
      body->add(box);
    }
  }
  add_footer();
}
