{
  stdenv,
  lib,
  enableWayland ? stdenv.hostPlatform.isLinux,
  enablePulseAudio ? stdenv.hostPlatform.isLinux,
  enableX11 ? stdenv.hostPlatform.isLinux,
  enableAlsa ? stdenv.hostPlatform.isLinux,
  enableSpeechd ? stdenv.hostPlatform.isLinux,
  enableMono ? false,
  enableVulkan ? true,
  pkg-config,
  autoPatchelfHook,
  installShellFiles,
  python3,
  scons,
  makeWrapper,
  libGL,
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
  eudev,
  alsa-lib,
  dbus,
  xcbuild,
  darwin,
  dotnet-sdk_9,
  dotnet-runtime_9,
}:

stdenv.mkDerivation {
  name = "redot-engine";
  src = ./.;

  nativeBuildInputs =
    [
      pkg-config
      autoPatchelfHook
      installShellFiles
      makeWrapper
      python3
      scons
    ]
    ++ lib.optionals enableMono [
      dotnet-sdk_9
    ]
    ++ lib.optionals enableWayland [
      wayland-scanner
    ];

  buildInputs =
    [
      fontconfig
      libGL
    ]
    ++ lib.optionals enableSpeechd [ speechd ]
    ++ lib.optionals enableVulkan [
      vulkan-loader
      vulkan-headers
    ]
    ++ lib.optionals enableWayland [
      wayland
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
      eudev
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
    )
    ++ lib.optionals enableMono [
      dotnet-runtime_9
    ];

  runtimeDependencies =
    [
      fontconfig.lib
      libGL
    ]
    ++ lib.optionals stdenv.targetPlatform.isLinux [
      eudev
      dbus
    ]
    ++ lib.optionals enableSpeechd [ speechd ]
    ++ lib.optionals enableVulkan [
      vulkan-loader
    ]
    ++ lib.optionals enableWayland [
      wayland
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
    ++ lib.optionals enableAlsa [ alsa-lib ];

  sconsFlags = lib.optionals stdenv.targetPlatform.isDarwin [
    "vulkan_sdk_path=${darwin.moltenvk}"
  ];

  installPhase = ''
    runHook preInstall
    mkdir -p "$out/bin"
    cp bin/redot*.* $out/bin/redot

    runHook postInstall
  '';
}
