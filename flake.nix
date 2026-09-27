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
        packages.default = pkgs.callPackage ./package.nix { };

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            llvm.clang
            llvm.llvm  # llvm-cov/llvm-profdata, version-matched to clang
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
      });
}
