{
  description = "wgwatch — top-like WireGuard monitor (C++20)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        llvm = pkgs.llvmPackages;
      in {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            llvm.clang
            gcc
            cmake
            ninja
            pkg-config
            fmt
            spdlog
            gtest
            libev
            gdb
          ];
          shellHook = ''
            export CC=${llvm.clang}/bin/clang
            export CXX=${llvm.clang}/bin/clang++
            echo "wgwatch dev shell: clang $(clang --version | head -1)"
          '';
        };

        packages.default = pkgs.stdenv.mkDerivation {
          pname = "wgwatch";
          version = "0.1.0";
          src = ./.;
          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config ];
          buildInputs = with pkgs; [ fmt spdlog gtest libev ];
          cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release" ];
        };
      });
}
