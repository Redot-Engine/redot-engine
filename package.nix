# Based on https://github.com/NixOS/nixpkgs/blob/6d5b297bc68f021d5fe9d43a3d1ba35a3b6d0663/pkgs/by-name/go/godot_4/package.nix

{
  alsa-lib,
  autoPatchelfHook,
  buildPackages,
  dbus,
  fontconfig,
  installShellFiles,
  lib,
  libdecor,
  libGL,
  libpulseaudio,
  libX11,
  libXcursor,
  libXext,
  libXfixes,
  libXi,
  libXinerama,
  libxkbcommon,
  libXrandr,
  libXrender,
  pkg-config,
  scons,
  speechd-minimal,
  stdenv,
  testers,
  udev,
  vulkan-loader,
  wayland,
  wayland-scanner,
  withDbus ? true,
  withFontconfig ? true,
  withPlatform ? "linuxbsd",
  withPrecision ? "single",
  withPulseaudio ? true,
  withSpeechd ? true,
  withTarget ? "editor",
  withTouch ? true,
  withUdev ? true,
  # Wayland in Redot requires X11 until upstream fix is merged
  # https://github.com/godotengine/godot/pull/73504
  withWayland ? true,
  withX11 ? true,
  src,
  commitHash,
  arch ? stdenv.hostPlatform.linuxArch,
  importEnvVars ? [ ],
}:
assert lib.asserts.assertOneOf "withPrecision" withPrecision [
  "single"
  "double"
];
let
  mkSconsFlagsFromAttrSet = lib.mapAttrsToList (
    k: v: if builtins.isString v then "${k}=${v}" else "${k}=${builtins.toJSON v}"
  );

  versionInfo = builtins.listToAttrs (
    map (
      e:
      let
        valuePair = lib.splitString " = " e;
      in
      {
        name = builtins.elemAt valuePair 0;
        value = lib.replaceStrings [ "\"" ] [ "" ] (builtins.elemAt valuePair 1);
      }
    ) (lib.splitString "\n" (builtins.readFile ./version.py))
  );

  version = "${versionInfo.major}.${versionInfo.minor}${
    lib.optionalString (versionInfo.patch != "0") ".${versionInfo.patch}"
  }-${versionInfo.status}";

  redotVersion = lib.replaceStrings [ "-" ] [ "." ] version;
in
stdenv.mkDerivation (finalAttrs: {
  pname = "redot4";

  inherit src version redotVersion;

  outputs = [
    "out"
    "man"
  ];
  separateDebugInfo = true;

  # Set the build name which is part of the version. In official downloads, this
  # is set to 'official'. When not specified explicitly, it is set to
  # 'custom_build'. Other platforms packaging Redot (Gentoo, Arch, Flatpak, NixOS
  # etc.) usually set this to their name as well.
  #
  # See also 'methods.py' in the Redot repo and 'build' in
  # https://docs.redotengine.org/en/stable/classes/class_engine.html#class-engine-method-get-version-info
  BUILD_NAME = "flake";

  # Required for the commit hash to be included in the version number.
  #
  # `methods.py` reads the commit hash from `.git/HEAD` and manually follows
  # refs. Since we just write the hash directly, there is no need to emulate any
  # other parts of the .git directory.
  #
  # See also 'hash' in
  # https://docs.redotengine.org/en/stable/classes/class_engine.html#class-engine-method-get-version-info
  preConfigure = ''
    mkdir -p .git
    echo ${commitHash} > .git/HEAD
  '';

  # From: https://github.com/Redot-Engine/redot-engine/blob/redot-4.3-stable/SConstruct
  sconsFlags = mkSconsFlagsFromAttrSet (
    {
      # Options from 'SConstruct'
      precision = withPrecision; # Floating-point precision level
      production = true; # Set defaults to build Redot for use in production
      platform = withPlatform;
      target = withTarget;
      import_env_vars = lib.foldl (a: b: "${a},${b}") "" importEnvVars;
      nix = true;

      inherit arch;
    }
    // (lib.optionalAttrs (withTarget == "editor") {
      # Options from 'platform/linuxbsd/detect.py'
      dbus = withDbus; # Use D-Bus to handle screensaver and portal desktop settings
      fontconfig = withFontconfig; # Use fontconfig for system fonts support
      pulseaudio = withPulseaudio; # Use PulseAudio
      speechd = withSpeechd; # Use Speech Dispatcher for Text-to-Speech support
      touch = withTouch; # Enable touch events
      udev = withUdev; # Use udev for gamepad connection callbacks
      wayland = withWayland; # Compile with Wayland support
      x11 = withX11; # Compile with X11 support
      module_mono_enabled = false;

      linkflags = "-Wl,--build-id";

      debug_symbols = true;
    })
  );

  enableParallelBuilding = true;

  strictDeps = true;

  depsBuildBuild = lib.optionals (stdenv.buildPlatform != stdenv.hostPlatform) [
    buildPackages.stdenv.cc
    pkg-config
  ];

  nativeBuildInputs = [
    autoPatchelfHook
    installShellFiles
    pkg-config
    scons
  ] ++ lib.optionals withWayland [ wayland-scanner ];

  runtimeDependencies =
    [
      alsa-lib
      libGL
      vulkan-loader
    ]
    ++ lib.optionals withX11 [
      libX11
      libXcursor
      libXext
      libXfixes
      libXi
      libXinerama
      libxkbcommon
      libXrandr
      libXrender
    ]
    ++ lib.optionals withWayland [
      libdecor
      wayland
    ]
    ++ lib.optionals withDbus [
      dbus
      dbus.lib
    ]
    ++ lib.optionals withFontconfig [
      fontconfig
      fontconfig.lib
    ]
    ++ lib.optionals withPulseaudio [ libpulseaudio ]
    ++ lib.optionals withSpeechd [ speechd-minimal ]
    ++ lib.optionals withUdev [ udev ];

  installPhase = ''
    runHook preInstall

    mkdir -p "$out/bin"
    cp bin/redot.* $out/bin/redot4

    installManPage misc/dist/linux/redot.6

    mkdir -p "$out"/share/{applications,icons/hicolor/scalable/apps}
    cp misc/dist/linux/org.redotengine.Redot.desktop "$out/share/applications/org.redotengine.Redot4.desktop"
    substituteInPlace "$out/share/applications/org.redotengine.Redot4.desktop" \
      --replace "Exec=redot" "Exec=$out/bin/redot4" \
      --replace "Redot Engine" "Redot Engine 4"
    cp icon.svg "$out/share/icons/hicolor/scalable/apps/redot.svg"
    cp icon.png "$out/share/icons/redot.png"

    runHook postInstall
  '';

  # see https://github.com/NixOS/nixpkgs/pull/400347
  dontAutoPatchelf = true;

  postFixup = ''
    autoPatchelf "$out"
  '';

  passthru.tests = {
    version = testers.testVersion {
      package = finalAttrs.finalPackage;
      version = redotVersion;
    };
  };

  requiredSystemFeatures = [
    # fixes: No space left on device
    "big-parallel"
  ];

  meta = {
    changelog = "https://github.com/Redot-Engine/redot-engine/releases/tag/${version}";
    description = "Free and Open Source 2D and 3D game engine";
    homepage = "https://redotengine.org";
    license = lib.licenses.mit;
    platforms = [
      "x86_64-linux"
      "aarch64-linux"
    ];
    mainProgram = "redot4";
  };
})
