#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include "cpptui.hpp"
#include "cli.hpp"
#include "parser.hpp"
#include "rates.hpp"

// Braille sparkline over a chronological rate history copy.
// rx = green, tx = cyan.
class BrailleGraph : public cpptui::Widget {
 public:
  BrailleGraph(std::array<double, 120> hist, std::size_t n, bool is_rx);
  void render(cpptui::Buffer& buffer) override;

 private:
  std::array<double, 120> hist_;
  std::size_t n_;
  bool is_rx_;
};

// Root layout owner. refresh() runs on the UI thread only (wired via
// App::post from the sampler callback). `q` quits, `k` toggles keys.
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
  WgFrame last_;
};
