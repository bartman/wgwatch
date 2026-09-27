#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cpptui.hpp"
#include "cli.hpp"
#include "parser.hpp"
#include "rates.hpp"
#include "theme.hpp"

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

// Bar plot cells, top row first: rows[r][x] is one glyph (" ", a partial
// eighth-block, or "█"). Empty cells stay " " (the widget paints the black
// background underneath). Pure: unit-tested.
inline std::vector<std::vector<const char*>> bar_rows(
    const std::array<double, 120>& hist, std::size_t n, int width,
    int height) {
  std::vector<std::vector<const char*>> rows;
  if (n < 1 || width <= 0 || height <= 0) return rows;
  double maxv = 0.0;
  for (std::size_t i = 0; i < n; ++i) maxv = std::max(maxv, hist[i]);
  if (maxv <= 0.0) maxv = 1.0;
  static constexpr const char* kParts[8] = {" ", "▁", "▂", "▃",
                                            "▄", "▅", "▆", "▇"};
  rows.assign(static_cast<std::size_t>(height),
              std::vector<const char*>(static_cast<std::size_t>(width), " "));
  const int span = width > 1 ? width - 1 : 1;
  for (int x = 0; x < width; ++x) {
    const std::size_t idx =
        (n <= 1) ? 0 : static_cast<std::size_t>(x * (n - 1) / span);
    const long eighths =
        std::lround(hist[idx] / maxv * height * 8.0);
    const long full = eighths / 8;
    const long rem = eighths % 8;
    for (int r = 0; r < height; ++r) {
      const long from_bottom = height - 1 - r;
      if (from_bottom < full)
        rows[static_cast<std::size_t>(r)][static_cast<std::size_t>(x)] = "█";
      else if (from_bottom == full && rem > 0)
        rows[static_cast<std::size_t>(r)][static_cast<std::size_t>(x)] =
            kParts[rem];
    }
  }
  return rows;
}

// One directed plot (rx or tx) confined to its own region. Line mode draws
// the braille line plot; bar mode stacks unicode blocks. The region is a
// solid black rectangle; the graph draws over it.
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

// Root layout owner. refresh() runs on the UI thread only (wired via
// App::post from the sampler callback). Keys: q quit, k keys, s sort,
// p plot mode, t theme.
class Ui {
 public:
  Ui(cpptui::App& app, CliOptions& opts, RateTracker& tracker);

  std::shared_ptr<cpptui::Widget> root() const { return root_; }
  void refresh(const WgFrame& frame);

 private:
  cpptui::App* app_;
  CliOptions* opts_;
  RateTracker* tracker_;
  std::shared_ptr<cpptui::Vertical> root_;
  std::shared_ptr<cpptui::Label> header_;
  std::shared_ptr<cpptui::Label> footer_;
  WgFrame last_;
};
