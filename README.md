# wgwatch

Top-like WireGuard monitor (C++20, CMake+Ninja, clang first).

## Build

```sh
# inside the nix dev shell (direnv allow / nix develop)
make
make test
```

Compiler: no hardcoding — `CXX=clang++` first, `CXX=g++` fallback.
Other targets: `make clean`, `make distclean`, `make help`.
Direct cmake also works: presets `ninja-release` (also `ninja-debug`
with `-Werror`, `ninja-asan`).

`make config` links `./compile_commands.json` to the BUILD dir one for editors.

## Run

```sh
./build/wgwatch [-u SEC] [-i IFACE] [-r [user@]host] [-s TYPE] [-v] [--log FILE] [--show-keys]
```

- `-u/--update SEC`: refresh interval, 0.1–3600s (default 1.0).
- `-i/--interface IFACE`: show one interface, or `all` (default).
- `-r/--remote [user@]host`: collect over ssh (key auth, `BatchMode=yes`).
- `-s/--sort TYPE`: peer order — `native`, `endpoint`, `allowed`,
  `mru` (default), `lru`, `rx-bytes`, `tx-bytes`, `bytes`, `rx-rate`,
  `tx-rate`, `rate` (`help` lists them).
- `-v/--verbose`: debug logging; repeat (`-vv`) for trace. Default info.
- `--log FILE`: write logs to FILE (truncated) instead of stderr.
- `--show-keys`: reveal WireGuard keys (hidden by default).

Needs permission to run `wg show`: root, or `sudo`/`doas` on PATH
(resolved inside the loop shell, locally and remotely). If the collector
goes quiet (no frames for ~3s or 2.5× the update interval), the display
shows an `ERR` banner instead of freezing silently.

## Keys / bindings

- `q`: quit (plus Ctrl+C).
- `k`: toggle key visibility.
- `s`: cycle peer sort order (shown in the status bar).
- Header shows peer count and key mode; stale collector shows an `ERR` banner.
- Peer rows: endpoint, allowed IPs, totals, live rx/tx rates, handshake age
  (green <3min, yellow <10min, red older), and rx/tx braille sparklines.
