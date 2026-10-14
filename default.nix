{ pkgs ? import <nixpkgs> {} }:
pkgs.stdenv.mkDerivation {
  pname = "linpaint";
  version = "6.1";
  # only files tracked by git, so local binaries/build dirs never leak in
  src = pkgs.lib.fileset.toSource {
    root = ./.;
    fileset = pkgs.lib.fileset.gitTracked ./.;
  };
  nativeBuildInputs = with pkgs; [ pkg-config wrapGAppsHook3 ];
  buildInputs = with pkgs; [ gtk3 librsvg ];
  makeFlags = [ "PREFIX=$(out)" ];
  installPhase = ''
    runHook preInstall
    make install PREFIX=$out
    runHook postInstall
  '';
}
