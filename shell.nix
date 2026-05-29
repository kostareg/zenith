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

    # SystemRDL
    python3Packages.peakrdl
    python3Packages.peakrdl-regblock
    python3Packages.peakrdl-markdown
  ];
in
  pkgs.mkShell {
    inherit buildInputs;
  }
