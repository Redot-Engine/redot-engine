{
  stdenv,
  lib,
  enableWayland ? stdenv.targetPlatform.isLinux,
  enablePulseAudio ? stdenv.targetPlatform.isLinux,
  enableX11 ? stdenv.targetPlatform.isLinux,
  enableAlsa ? stdenv.targetPlatform.isLinux,
  enableSpeechd ? stdenv.targetPlatform.isLinux,
  enableVulkan ? true,
  pkgconf,
  autoPatchelfHook,
  installShellFiles,
  python3,
  scons,
  makeWrapper,
  vulkan-loader,
  vulkan-headers,
  speechd,
  fontconfig,
  wayland,
  wayland-scanner,
  wayland-protocols,
  libdecor,
  libxkbcommon,
  libpulseaudio,
  xorg,
  udev,
  alsa-lib,
  dbus,
  xcbuild,
  darwin,
}:

stdenv.mkDerivation {
  name = "redot-engine";
  src = ./.;

  buildInputs =
    [
      pkgconf
      autoPatchelfHook
      installShellFiles
      python3
      makeWrapper
      scons
      fontconfig
    ]
    ++ lib.optionals enableSpeechd [ speechd ]
    ++ lib.optionals enableVulkan [
      vulkan-loader
      vulkan-headers
    ]
    ++ lib.optionals enableWayland [
      wayland
      wayland-scanner
      wayland-protocols
      libdecor
      libxkbcommon
    ]
    ++ lib.optionals enablePulseAudio [ libpulseaudio ]
    ++ lib.optionals enableX11 [
      xorg.libX11
      xorg.libXcursor
      xorg.libXinerama
      xorg.libXext
      xorg.libXrandr
      xorg.libXrender
      xorg.libXi
      xorg.libXfixes
    ]
    ++ lib.optionals enableAlsa [ alsa-lib ]
    ++ lib.optionals stdenv.targetPlatform.isLinux [
      udev
      dbus
    ]
    ++ lib.optionals stdenv.targetPlatform.isDarwin [
      xcbuild
      darwin.moltenvk
    ]
    ++ lib.optionals stdenv.targetPlatform.isDarwin (
      with darwin.apple_sdk.frameworks;
      [
        AppKit
      ]
    );

  sconsFlags = lib.optionals stdenv.targetPlatform.isDarwin [
    "vulkan_sdk_path=${darwin.moltenvk}"
  ];
}
