#!/usr/bin/env bash
# Install wgwatch build dependencies via apt or dnf.
# Unknown environment: warn and skip (nix develop already provides everything).
set -u

msg() { printf 'dependencies.sh: %s\n' "$*" >&2; }

if command -v apt-get >/dev/null 2>&1; then
  sudo apt-get update && sudo apt-get install -y \
    clang cmake ninja-build pkg-config \
    libev-dev libfmt-dev libspdlog-dev libgtest-dev
elif command -v dnf >/dev/null 2>&1; then
  sudo dnf install -y \
    clang cmake ninja-build pkg-config \
    libev-devel fmt-devel spdlog-devel gtest-devel
else
  msg "no apt-get/dnf found; skipping (use nix develop for deps)"
fi
