{pkgs}: let
  buildInputs = with pkgs; [
    # C++
    gcc
    cmake

    # Web development
    bun
    emscripten
  ];
in
  pkgs.mkShell {
    inherit buildInputs;
  }
