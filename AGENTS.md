# AGENTS.md — wgwatch

## Commands

- Build: `make` (`BUILD=build TYPE=release CXX=clang++` overrides).
- Test: `make test` (or `ctest --test-dir build --output-on-failure`).
- Direct cmake: presets `ninja-release` / `ninja-debug` / `ninja-asan`.
- Compiler: `CXX=clang++` first, `CXX=g++` fallback. Never commit without asking.

## Map

- `src/cli.*`: option parsing/validation (`--sort` types, `-v`/`--log`,
  full `--help`).
- `src/priv.hpp`: is_root + sudo snippet.
- `src/collector.*`: loop-command builder + forked `sh -c` owner (own process
  group, group kill on stop).
- `src/parser.*`: `wg show all dump` frame parser (all consumers use it).
- `src/rates.*`: per-peer rates + 120-sample histories (mutex-guarded) +
  `sort_peers()` display ordering.
- `src/format.hpp`: byte/rate/age formatting.
- `src/sampler.*`: libev reader thread (only file including `<ev.h>`);
  emits stale frames on EOF or stall (>2.5× update, min 3s).
- `src/ui.*`, `src/main.cpp`: boxed peer layout, PlotWidget (line/bar),
  dynamic plot height (`plot_height_for`), `t`/`p` keys; spdlog setup
  lives in main (`-v` → debug, `-vv` → trace, `--log` → file).
[AGENTS.md#A685]
- `cmake/*.cmake`: FetchContent packages (SYSTEM + FIND_PACKAGE_ARGS).
- `src/theme.hpp`: 12 built-in themes (terminalcolors.com palettes);
  `theme_at()` wraps for the `t` key.
- `src/config.hpp`: `~/.config/wgwatch/config` load/save (atomic
  rewrite); toggles persist, CLI flags override the file.
- `tests/fake-wg.py`: fake `wg show all dump` for demos
  (`--command`, env-tuned loads, `.fake-wg-state` counters).
- `package.nix` / `default.nix`: nix derivation + non-flake entry
- `RELEASE.md`: release checklist (bump → tag → push).

## Rules

- Makefile wraps cmake only (`cmake -S/-B`, `cmake --build`, `ctest`); the
  `ln -s` for `compile_commands.json` is the one exception.
- Tests read fixtures from `_attic/` via `WG_ATTIC_DIR`; no `tests/fixtures/`.
- Trace points use function-form `spdlog::trace` (always compiled); keep
  them to sizes/counts, never key material.
- `README.md` is user-facing: keep install commands copy-pasteable with
  exact package filenames; `RELEASE.md` owns the release procedure.
