{ lib, stdenv, cmake, ninja, pkg-config, fmt, spdlog, gtest, libev }:

stdenv.mkDerivation {
  pname = "wgwatch";
  version = "0.1.1";
  src = lib.cleanSource ./.;
  nativeBuildInputs = [ cmake ninja pkg-config ];
  buildInputs = [ fmt spdlog gtest libev ];
  cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release" ];
}
