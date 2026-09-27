#!/usr/bin/env bash
# Install wgwatch build dependencies via apt or dnf.
# Unknown environment: warn and skip (nix develop already provides everything).
# --coverage: also install llvm-cov/llvm-profdata for `make coverage`.
set -u

msg() { printf 'dependencies.sh: %s\n' "$*" >&2; }

SUDO=""
if [ "$(id -u)" -ne 0 ] && command -v sudo >/dev/null 2>&1; then
  SUDO="sudo"
fi

COV=""
if [ "${1:-}" = "--coverage" ]; then
  # llvm-cov/llvm-profdata for `make coverage`, curl+gpg for codecov-action.
  COV="llvm curl gnupg"
elif [ -n "${1:-}" ]; then
  msg "unknown option '$1' (expected --coverage)"
  exit 1
fi

if command -v apt-get >/dev/null 2>&1; then
  # shellcheck disable=SC2086
  $SUDO apt-get update && $SUDO apt-get install -y \
    clang g++ cmake ninja-build pkg-config git ca-certificates dpkg-dev file \
    libev-dev libfmt-dev libspdlog-dev libgtest-dev $COV
elif command -v dnf >/dev/null 2>&1; then
  # shellcheck disable=SC2086
  $SUDO dnf install -y \
    clang gcc-c++ cmake ninja-build pkg-config git ca-certificates rpm-build \
    libev-devel fmt-devel spdlog-devel gtest-devel $COV
else
  msg "no apt-get/dnf found; skipping (use nix develop for deps)"
fi
