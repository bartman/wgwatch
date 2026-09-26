#include <exception>
#include <string>

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "cli.hpp"
#include "collector.hpp"
#include "cpptui.hpp"
#include "rates.hpp"
#include "sampler.hpp"
#include "ui.hpp"

namespace {

void setup_logging(const CliOptions& o) {
  if (o.log_file) {
    auto sink =
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(*o.log_file, true);
    spdlog::set_default_logger(
        std::make_shared<spdlog::logger>("wgwatch", sink));
  }
  spdlog::level::level_enum level = spdlog::level::info;
  if (o.verbose >= 2)
    level = spdlog::level::trace;
  else if (o.verbose == 1)
    level = spdlog::level::debug;
  spdlog::set_level(level);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
}

}  // namespace

int main(int argc, char* argv[]) {
  try {
    CliOptions opts = parse_cli(argc, argv);
    setup_logging(opts);
    spdlog::info(
        "wgwatch starting: update={:.3f}s interface={} remote={} sort={} "
        "keys={} verbose={} log={}",
        opts.update_sec, opts.interface,
        opts.remote ? *opts.remote : std::string("local"),
        sort_name(opts.sort), opts.show_keys ? "shown" : "hidden",
        opts.verbose,
        opts.log_file ? *opts.log_file : std::string("stderr"));
    cpptui::App app;
    RateTracker tracker;
    Ui ui(app, opts, tracker);
    Collector col;
    col.start(opts);
    const double stall_after =
        opts.update_sec * 2.5 > 3.0 ? opts.update_sec * 2.5 : 3.0;
    Sampler sampler(col.fd(), opts.interface, stall_after, tracker,
                    [&](WgFrame f) {
                      app.post([&, f = std::move(f)]() mutable {
                        spdlog::trace("ui: frame t={:.3f} peers={} stale={}",
                                      f.tstamp, f.peers.size(), f.stale);
                        ui.refresh(f);
                      });
                    });
    sampler.start();
    spdlog::info("entering UI loop");
    app.run(ui.root());
    spdlog::info("UI loop exited, stopping");
    sampler.stop();
    col.stop();
    spdlog::info("wgwatch stopped");
  } catch (const HelpRequested&) {
    fmt::print("{}", usage());
    return 0;
  } catch (const SortHelpRequested&) {
    fmt::print("{}", sort_help());
    return 0;
  } catch (const std::exception& e) {
    spdlog::error("{}", e.what());
    return 1;
  }
  return 0;
}
