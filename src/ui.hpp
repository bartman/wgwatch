#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "cpptui.hpp"
#include "cli.hpp"
#include "parser.hpp"
#include "rates.hpp"
#include "theme.hpp"

// Plot height: every peer box costs 5 fixed lines (top/bottom border, info,
// allowed-ips, stats) plus h plot lines; each interface costs 1 header line;
// the header + footer cost 1 line each. All plots share one height so boxes
// align: min 1, max 4 lines.
inline int plot_height_for(int term_rows, int n_ifaces, int n_peers) {
  if (n_peers <= 0) return 4;
  const int h = (term_rows - 2 - n_ifaces - 5 * n_peers) / n_peers;
  if (h < 1) return 1;
  if (h > 4) return 4;
  return h;
}

// Bar plot cells, top row first: rows[r][x] is one glyph (" ", a partial
// eighth-block, or "█"). Empty cells stay " " (transparent).
// Pure: unit-tested.
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
