# wgwatch

[![CI](https://github.com/bartman/wgwatch/actions/workflows/ci.yml/badge.svg)](https://github.com/bartman/wgwatch/actions) [![codecov](https://codecov.io/gh/bartman/wgwatch/branch/master/graph/badge.svg)](https://codecov.io/gh/bartman/wgwatch)

Top-like WireGuard monitor (C++20, CMake+Ninja, clang first).

## Quickstart

Debian / Fedora:

```sh
./dependencies.sh   # apt/dnf installs: compiler, cmake, ninja, libev, fmt, spdlog, gtest
make
make install        # installs to ~/.local/bin
wgwatch
```

Nix dev shell:

```sh
nix develop         # or: direnv allow
make
./build/wgwatch
```

Nix flake — try it without installing anything:

```sh
nix run github:bartman/wgwatch -- --help
```

Use wgwatch from your own flake / NixOS config:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";
    wgwatch.url = "github:bartman/wgwatch";
  };

  outputs = { nixpkgs, wgwatch, ... }:
    let system = "x86_64-linux";
    in {
      nixosConfigurations.myhost = nixpkgs.lib.nixosSystem {
        inherit system;
        modules = [
          {
            environment.systemPackages = [
              wgwatch.packages.${system}.default
            ];
          }
        ];
      };
    };
}
```

Without flakes: `nix-build` puts the binary at `./result/bin/wgwatch`
(see `default.nix`).

## Packages

Automated .deb and .rpm builds are available for download and manual install.
These have not been thoroughly tested.

https://github.com/bartman/wgwatch/releases/latest

On Debian based systems (from the download directory):
```sh
sudo apt install ./wgwatch_0.1.4_amd64.deb
```

On Fedora based systems (from the download directory):
```sh
sudo dnf install ./wgwatch-0.1.4-1.x86_64.rpm
```

## Options

```sh
wgwatch [-u SEC] [-i IFACE] [-r [user@]host] [-s TYPE] [-v] [--log FILE] [--show-keys]
```

- `-u/--update SEC`: refresh interval, 0.1–3600s (default 1.0).
- `-i/--interface IFACE`: show one interface, or `all` (default).
- `-r/--remote [user@]host`: collect over ssh (key auth, `BatchMode=yes`).
- `--command PATH`: wg binary to run, locally and remotely (default `wg`).
- `-s/--sort TYPE`: peer order — `native`, `endpoint`, `allowed`,
  `mru` (default), `lru`, `rx-bytes`, `tx-bytes`, `bytes`, `rx-rate`,
  `tx-rate`, `rate` (`help` lists them).
- `-v/--verbose`: debug logging; repeat (`-vv`) for trace. Default info.
- `--log FILE`: write logs to FILE (truncated) instead of stderr.
- `--show-keys`: reveal WireGuard keys (hidden by default).
- `--hide-inactive`: hide peers that never handshook (shown by default).

Needs permission to run `wg show`: root, or `sudo`/`doas` on PATH
(resolved inside the loop shell, locally and remotely). If the collector
goes quiet (no frames for ~3s or 2.5× the update interval), the display
shows an `ERR` banner instead of freezing silently.

## Keys / bindings

- `q`: quit (plus Ctrl+C).
- `k`: toggle key visibility.
- `s`: cycle peer sort order (shown in the footer).
- `i`: toggle inactive peers (never handshook) shown/hidden.
- `p`: toggle plot style (`line` braille vs `bar` blocks).
- `t`: cycle theme (catppuccin-mocha, dracula, nord, gruvbox-dark,
  tokyo-night, solarized-dark, github-dark, shades-of-purple, rose-pine,
  kanagawa-wave, everforest-dark, one-dark).
- Header shows origin, peer count, and window; footer shows sort, plot,
  theme, and key mode. Stale collector shows an `ERR` banner.
- Each peer gets a box: endpoint, handshake age (green <3min, yellow
  <10min, red older), key, allowed IPs, side-by-side rx/tx plots, and
  totals with window avg/peak rates. Plot height (1–4 lines) scales to
  fit the terminal; when peers still overflow, boxes compress (shared
  separators, merged info line) and long lines truncate with ….

## Config

UI options persist in `~/.config/wgwatch/config` (`key = value` lines).
Any toggle key (`k`, `s`, `i`, `p`, `t`) rewrites it atomically
(staging file + rename). The file is re-read on startup; command-line
flags override it. `remote` and `interface` are never written (always
`local` / `all` by default).
