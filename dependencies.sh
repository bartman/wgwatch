#!/usr/bin/env bash
# Install wgwatch build dependencies via apt or dnf.
# Unknown environment: warn and skip (nix develop already provides everything).
set -u

msg() { printf 'dependencies.sh: %s\n' "$*" >&2; }

SUDO=""
if [ "$(id -u)" -ne 0 ] && command -v sudo >/dev/null 2>&1; then
  SUDO="sudo"
fi

if command -v apt-get >/dev/null 2>&1; then
  $SUDO apt-get update && $SUDO apt-get install -y \
    clang cmake ninja-build pkg-config git ca-certificates \
    libev-dev libfmt-dev libspdlog-dev libgtest-dev
elif command -v dnf >/dev/null 2>&1; then
  $SUDO dnf install -y \
    clang cmake ninja-build pkg-config git ca-certificates \
    libev-devel fmt-devel spdlog-devel gtest-devel
else
  msg "no apt-get/dnf found; skipping (use nix develop for deps)"
fi
