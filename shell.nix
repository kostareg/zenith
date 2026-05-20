{pkgs}: let
  buildInputs = with pkgs; [
    # C++
    gcc # todo: clang
    cmake
    ninja
    clang-tools

    # Web development
    bun
    emscripten
  ];
in
  pkgs.mkShell {
    inherit buildInputs;
  }
