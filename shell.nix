{
  pkgs ? import <nixpkgs> { },
}:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    llvmPackages.clang-tools
    cmake
    ninja
    pkg-config
    gcc
    gdb
  ];

  buildInputs = with pkgs; [
    qt6.qtbase
    kdePackages.dolphin
    kdePackages.kio
    kdePackages.kcoreaddons
    kdePackages.ki18n
  ];

  shellHook = ''
    echo "C++ Development Environment Active (Nix Channels)"
  '';
}
