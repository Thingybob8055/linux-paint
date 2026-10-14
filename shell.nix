{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell { packages = with pkgs; [ gtk3 pkg-config gnumake gcc ]; }
