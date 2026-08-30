{
  lib,
  stdenv,
  cmake,
  qt6,
  kdePackages,
  icoutils,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "dolphin-steam-prefix-extension";
  version = "1.0.0";

  src = ./..;

  buildInputs = [
    qt6.qtbase
    kdePackages.extra-cmake-modules
    kdePackages.kbookmarks
    kdePackages.kcoreaddons
    kdePackages.kio
  ];

  nativeBuildInputs = [
    cmake
  ];

  postPatch = ''
    substituteInPlace src/steamhelper.h \
      --replace-fail 'QStringLiteral("wrestool")' 'QStringLiteral("${icoutils}/bin/wrestool")' \
      --replace-fail 'QStringLiteral("icotool")' 'QStringLiteral("${icoutils}/bin/icotool")'
  '';

  dontWrapQtApps = true;

  meta = {
    description = "KDE Dolphin plugin that extends Steam compatdata folder functionality";
    homepage = "https://github.com/FelixSmtt/dolphin-steam-prefix-extension";
    license = lib.licenses.gpl3Only;
    platforms = lib.platforms.linux;
  };
})
