{pkgs}: let
  buildInputs = with pkgs; [
    # C++
    gcc
    cmake
    ninja

    # Web development
    bun
    emscripten
  ];
in
  pkgs.mkShell {
    inherit buildInputs;
  }
