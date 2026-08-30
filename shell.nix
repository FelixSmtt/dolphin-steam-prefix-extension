{
  pkgs ? import <nixpkgs> { },
}:

let
  projectRoot = toString ./.;

  dolphinl = pkgs.writeShellScriptBin "dolphinl" ''
    echo "Launching Dolphin (logging to ${projectRoot}/dolphin.log)..."
    exec dolphin "$@" 2>&1 | tee "${projectRoot}/dolphin.log"
  '';
in
pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    dolphinl
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


    # Set environment variables for Qt and KDE
    echo "Setting up environment variables for Qt and KDE..."
    mkdir -p "${projectRoot}/install_test/lib64/plugins" "${projectRoot}/install_test/share"
    export QT_PLUGIN_PATH="${projectRoot}/install_test/lib64/plugins:$QT_PLUGIN_PATH"
    export XDG_DATA_DIRS="${projectRoot}/install_test/share:$XDG_DATA_DIRS"

    export QT_DEBUG_PLUGINS=1
  '';
}
