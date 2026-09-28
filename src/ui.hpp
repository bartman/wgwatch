#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>

#include "cpptui.hpp"
#include "cli.hpp"
#include "parser.hpp"
#include "rates.hpp"
#include "theme.hpp"
#include "unicode_utils.hpp"

// Plot height (normal mode): every peer box costs 5 fixed lines (top/bottom
// border, info, allowed-ips, stats) plus h plot lines; each interface run
// costs 1 header line; the header + footer cost 1 line each. All plots share
// one height so boxes align: min 1, max 4 lines.
inline int plot_height_for(int term_rows, int n_ifaces, int n_peers) {
  if (n_peers <= 0) return 4;
  const int h = (term_rows - 2 - n_ifaces - 5 * n_peers) / n_peers;
  if (h < 1) return 1;
  if (h > 4) return 4;
  return h;
}

// Plot height (compressed mode): peers share one box per interface run, so
// each peer costs 2 fixed lines (info, stats) plus h plot lines, and each
// run costs 1 header + (peers + 1) border/separator lines.
inline int compressed_plot_height(int term_rows, int n_runs, int n_peers) {
  if (n_peers <= 0) return 4;
  const int h = (term_rows - 2 - 2 * n_runs - 3 * n_peers) / n_peers;
  if (h < 1) return 1;
  if (h > 4) return 4;
  return h;
}

// One colored run of a status line (no fg = widget default).
struct TextRun {
  std::string text;
  std::optional<cpptui::Color> fg;
};

// Truncate plain text to max_w display columns, appending "…" (pure).
inline std::string fit_text(const std::string& s, int max_w) {
  if (max_w <= 0) return "";
  if (cpptui::utf8_display_width(s) <= max_w) return s;
  if (max_w == 1) return "…";
  std::string out;
  std::size_t pos = 0;
  int w = 0;
  while (pos < s.size()) {
    uint32_t cp = 0;
    int len = 0;
    if (!cpptui::TextHelper::utf8_decode_codepoint(s, pos, cp, len) ||
        len <= 0)
      break;
    // +1 reserves the trailing "…".
    if (w + cpptui::char_display_width(cp) + 1 > max_w) break;
    out.append(s, pos, static_cast<std::size_t>(len));
    w += cpptui::char_display_width(cp);
    pos += static_cast<std::size_t>(len);
  }
  out += "…";
  return out;
}

// Truncate runs to max_w columns, keeping each run's color (pure).
inline std::vector<TextRun> fit_runs(std::vector<TextRun> runs, int max_w) {
  std::vector<TextRun> out;
  int w = 0;
  for (auto& r : runs) {
    if (w >= max_w) break;
    const int rw = cpptui::utf8_display_width(r.text);
    if (w + rw <= max_w) {
      w += rw;
      out.push_back(std::move(r));
      continue;
    }
    r.text = fit_text(r.text, max_w - w);
    out.push_back(std::move(r));
    break;
  }
  return out;
}

inline cpptui::StyledText to_styled(const std::vector<TextRun>& runs) {
  cpptui::StyledText st;
  for (const auto& r : runs) {
    if (r.fg)
      st.colored(r.text, *r.fg);
    else
      st.add(r.text);
  }
  return st;
}

// Bar plot cells at octant resolution: each cell covers 2 sample columns
// x 4 sub-rows (4x the vertical and 2x the horizontal resolution of
// whole-cell blocks). Bars fill from the measured level down, so every
// cell is blank, full (U+2588), or one bottom-filled partial from
// wuni::kOctant; horizontally adjacent non-empty buckets always touch,
// and only true data zeros leave gaps. Empty cells stay " " (the widget
// paints the black background underneath). Pure: unit-tested.
inline std::vector<std::vector<std::string>> bar_rows(
    const std::array<double, 120>& hist, std::size_t n, int width,
    int height) {
  std::vector<std::vector<std::string>> rows;
  if (n < 1 || width <= 0 || height <= 0) return rows;
  double maxv = 0.0;
  for (std::size_t i = 0; i < n; ++i) maxv = std::max(maxv, hist[i]);
  if (maxv <= 0.0) maxv = 1.0;
  const int cols = width * 2;
  const int sub = height * 4;
  // Max-downsample so spikes survive: bucket b covers
  // hist [b*n/cols, (b+1)*n/cols). Buckets narrower than one sample
  // reuse their sample; the result still touches.
  std::vector<double> level(static_cast<std::size_t>(cols), 0.0);
  for (int b = 0; b < cols; ++b) {
    std::size_t i0 =
        static_cast<std::size_t>(b) * n / static_cast<std::size_t>(cols);
    std::size_t i1 = static_cast<std::size_t>(b + 1) * n /
                     static_cast<std::size_t>(cols);
    if (i1 > n) i1 = n;
    double m = 0.0;
    for (std::size_t i = i0; i < i1; ++i) m = std::max(m, hist[i]);
    if (i1 <= i0 && i0 < n) m = hist[i0];
    level[static_cast<std::size_t>(b)] = m;
  }
  rows.assign(static_cast<std::size_t>(height),
              std::vector<std::string>(static_cast<std::size_t>(width), " "));
  for (int cx = 0; cx < width; ++cx) {
    for (int cy = 0; cy < height; ++cy) {
      int pat = 0;
      for (int sx = 0; sx < 2; ++sx) {
        const double v =
            level[static_cast<std::size_t>(cx * 2 + sx)];
        long fill = v <= 0.0 ? 0 : std::lround(v / maxv * sub);
        if (fill > sub) fill = sub;
        for (int sr = 0; sr < 4; ++sr) {
          if (cy * 4 + sr >= sub - fill) {
            // Braille dots: rows 0-2 are bits r / r+3 per column,
            // row 3 is bits 6 / 7.
            const int bit =
                (sr < 3) ? (sr + (sx != 0 ? 3 : 0)) : (sx != 0 ? 7 : 6);
            pat |= 1 << bit;
          }
        }
      }
      if (pat == 0) continue;
      rows[static_cast<std::size_t>(cy)][static_cast<std::size_t>(cx)] =
          wuni::utf8_encode(wuni::kOctant[pat]);
    }
  }
  return rows;
}

// One directed plot (rx or tx) confined to its own region. Line mode draws
// the braille line plot; bar mode fills octant block bars (2x4 resolution).
// The region is a solid black rectangle; the graph draws over it.
class PlotWidget : public cpptui::Widget {
 public:
  PlotWidget(std::array<double, 120> hist, std::size_t n, PlotMode mode,
             cpptui::Color fg);
  void render(cpptui::Buffer& buffer) override;

 private:
  std::array<double, 120> hist_;
  std::size_t n_;
  PlotMode mode_;
  cpptui::Color fg_;
};

// One shared box per interface run (compressed mode). Paints the peer
// separators itself, overpainting the outer │ cells so the ├─┤ tees blend
// into the box: a child widget cannot do this because Border clips children
// to the inner rect.
class CompressedBox : public cpptui::Border {
 public:
  CompressedBox(int peers, int plot_h, cpptui::Color c)
      : cpptui::Border(cpptui::BorderStyle::Single, c) {
    fixed_height = 2 + peers * (2 + plot_h) + (peers - 1);
    // Inner rows (relative to the box top) carrying a separator: each
    // peer takes 1 info + plot_h + 1 stats lines.
    for (int k = 0; k + 1 < peers; ++k)
      seps_.push_back(1 + k * (3 + plot_h) + (2 + plot_h));
  }
  void render(cpptui::Buffer& buffer) override {
    Border::render(buffer);
    if (width < 2) return;
    cpptui::Color fg = color_.resolve(cpptui::Theme::current().border);
    cpptui::Color bg = bg_color_.resolve(cpptui::Theme::current().background);
    for (int r : seps_) {
      if (r < 1 || r >= height - 1) continue;  // never paint past a clip
      cpptui::Cell cell;
      cell.fg_color = fg;
      cell.bg_color = bg;
      cell.content = "├";
      buffer.set(x, y + r, cell);
      cell.content = "─";
      for (int cx = 1; cx < width - 1; ++cx) buffer.set(x + cx, y + r, cell);
      cell.content = "┤";
      buffer.set(x + width - 1, y + r, cell);
    }
  }

 private:
  std::vector<int> seps_;
};

// One collector-stderr line with its arrival stamp. TTL is enforced per
// refresh (see kErrTtlSec).
struct ErrLine {
  std::chrono::steady_clock::time_point at;
  std::string text;
};

// Overlay container pinned just above the 1-row footer: its single child
// keeps full width and its caller-set fixed_height, sitting at
// y + height - 1 - H (the -1 reserves the footer row). Container::render
// already skips invisible children, so visible=false fully hides it.
class BottomPin : public cpptui::Container {
 public:
  void layout() override {
    for (auto& child : children_) {
      if (!child->visible) continue;
      child->x = x;
      child->width =
          child->fixed_width > 0
              ? child->fixed_width
              : clamp_size(width, child->min_width, child->max_width);
      const int h = child->fixed_height > 0 ? child->fixed_height : height;
      child->height = h;
      child->y = y + height - 1 - h;
      if (auto cont = std::dynamic_pointer_cast<Container>(child))
        cont->layout();
    }
  }
};

// Root layout owner. refresh() runs on the UI thread only (wired via
// App::post from the sampler callback). Keys: q quit, k keys, s sort,
// p plot mode, t theme.
class Ui {
 public:
  Ui(cpptui::App& app, CliOptions& opts, RateTracker& tracker);

  std::shared_ptr<cpptui::Widget> root() const { return root_; }
  void refresh(const WgFrame& frame);
  // UI thread only; stamps steady_clock::now(). Shown until kErrTtlSec.
  void push_error(std::string text) {
    errors_.push_back({std::chrono::steady_clock::now(), std::move(text)});
  }

  static constexpr double kErrTtlSec = 5.0;

 private:
  cpptui::App* app_;
  CliOptions* opts_;
  RateTracker* tracker_;
  std::shared_ptr<cpptui::Stack> root_;
  std::shared_ptr<cpptui::Vertical> content_;
  std::shared_ptr<cpptui::Label> header_;
  std::shared_ptr<cpptui::Label> footer_;
  std::shared_ptr<BottomPin> err_overlay_;
  std::deque<ErrLine> errors_;
  WgFrame last_;
};

// Greedy codepoint wrap at max_w display columns, hard-splitting mid-word.
// Empty input (or max_w <= 0) -> empty vector. Pure: unit-tested.
inline std::vector<std::string> wrap_lines(const std::string& text,
                                           int max_w) {
  std::vector<std::string> rows;
  if (text.empty() || max_w <= 0) return rows;
  std::string cur;
  int w = 0;
  std::size_t pos = 0;
  while (pos < text.size()) {
    uint32_t cp = 0;
    int len = 0;
    if (!cpptui::utf8_decode_codepoint(text, pos, cp, len) || len <= 0) break;
    const int cw = cpptui::char_display_width(cp);
    if (w + cw > max_w && !cur.empty()) {
      rows.push_back(cur);
      cur.clear();
      w = 0;
      continue;
    }
    cur.append(text, pos, static_cast<std::size_t>(len));
    w += cw;
    pos += static_cast<std::size_t>(len);
  }
  if (!cur.empty()) rows.push_back(cur);
  return rows;
}

// Drop entries older than kErrTtlSec. Pure: unit-tested.
inline void prune_errors(std::deque<ErrLine>& errors,
                         std::chrono::steady_clock::time_point now) {
  while (!errors.empty() &&
         std::chrono::duration<double>(now - errors.front().at).count() >
             Ui::kErrTtlSec)
    errors.pop_front();
}

// Wrap every record at max_w columns and flatten to display rows. Pure:
// unit-tested.
inline std::vector<std::string> error_box_rows(
    const std::deque<ErrLine>& errors, int max_w) {
  std::vector<std::string> rows;
  for (const auto& e : errors)
    for (auto& r : wrap_lines(e.text, max_w)) rows.push_back(std::move(r));
  return rows;
}
