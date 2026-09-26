# Non-flake entry point: `nix-build` → ./result/bin/wgwatch
{ pkgs ? import <nixpkgs> { } }:

pkgs.callPackage ./package.nix { }
