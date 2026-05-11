{
  description = "A Nix-flake-based C/C++ development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

  outputs = {
    self,
    nixpkgs,
  }: let
    supportedSystems = ["x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin"];
    forEachSupportedSystem = f:
      nixpkgs.lib.genAttrs supportedSystems (system:
        f rec {
          pkgs = import nixpkgs {inherit system;};
          isDarwin = pkgs.lib.hasSuffix system "darwin";
          arch = if pkgs.stdenv.hostPlatform.isAarch64 then "arm64" else "x86_64";

          linuxDeps = with pkgs; [
            autoPatchelfHook
            xorg.libX11
            xorg.libXcursor
            xorg.libXinerama
            xorg.libXext
            xorg.libXrandr
            xorg.libXrender
            xorg.libXi
            xorg.libXfixes
            libxkbcommon
            wayland-scanner
            wayland
            libdecor
            alsa-lib
            libpulseaudio
            udev
            dbus
            dbus.lib
          ];

          darwinDeps = with pkgs; [
            Foundation
            Cocoa
            AudioToolbox
            CoreAudio
            CoreVideo
            AVFoundation
          ];

          commonDeps = with pkgs; [
            stdenv.cc
            gcc-unwrapped
            pkg-config
            installShellFiles
            python3
            speechd
            makeWrapper
            mono
            dotnet-sdk_8
            dotnet-runtime_8
            vulkan-loader
            libGL
            fontconfig
            fontconfig.lib
            scons
          ];

          deps = if isDarwin then darwinDeps ++ commonDeps else linuxDeps ++ commonDeps;
          libraryPathVar = if isDarwin then "DYLD_LIBRARY_PATH" else "LD_LIBRARY_PATH";
          platform = if isDarwin then "macos" else "linuxbsd";
        });
  in {
    apps = forEachSupportedSystem ({
      pkgs,
      deps,
      libraryPathVar,
      platform,
      arch,
      ...
    }: let
      script = pkgs.writeShellScript "redot" ''
        export PATH=${pkgs.lib.makeBinPath deps}:$PATH
        export ${libraryPathVar}=${pkgs.lib.makeLibraryPath deps}

        scons_args=()
        runtime_args=()
        parsing_runtime_args=0
        show_help=0
        target=editor
        target_arch=${arch}
        scons_platform=${platform}
        dev_build=no
        precision=single
        threads=yes
        extra_suffix=

        for arg in "$@"; do
          if [ "$parsing_runtime_args" -eq 0 ] && [ "$arg" = "--help" ]; then
            show_help=1
            continue
          fi

          if [ "$parsing_runtime_args" -eq 0 ] && [ "$arg" = "--" ]; then
            parsing_runtime_args=1
            continue
          fi

          if [ "$parsing_runtime_args" -eq 0 ]; then
            case "$arg" in
              target=*) target=''${arg#target=} ;;
              arch=*)
                target_arch=''${arg#arch=}
                if [ "$target_arch" = "auto" ]; then
                  target_arch=${arch}
                fi
                ;;
              platform=*)
                scons_platform=''${arg#platform=}
                continue
                ;;
              dev_build=*) dev_build=''${arg#dev_build=} ;;
              precision=*) precision=''${arg#precision=} ;;
              threads=*) threads=''${arg#threads=} ;;
              extra_suffix=*) extra_suffix=''${arg#extra_suffix=} ;;
            esac

            scons_args+=("$arg")
          else
            runtime_args+=("$arg")
          fi
        done

        if [ "$show_help" -eq 1 ]; then
          cat <<EOF
Usage:
  nix run .
  nix run . -- [scons build flags...]
  nix run . -- [scons build flags...] -- [redot runtime args...]

Examples:
  nix run .
  nix run . -- target=editor dev_build=yes num_jobs=12
  nix run . -- target=template_release production=yes
  nix run . -- target=editor dev_build=yes -- --path /tmp/project

Argument handling:
  - The first '--' is consumed by Nix.
  - Arguments before the next '--' are passed to SCons.
  - Arguments after the next '--' are passed to the Redot binary.

Notes:
  - Common SCons flags include target=..., dev_build=yes, production=yes,
    module_mono_enabled=yes, precision=double, num_jobs=..., and ccflags=...
  - The wrapper auto-builds only when a matching binary is not already present.
    Use 'nix develop' for full manual rebuild control.
EOF
          exit 0
        fi

        binary_pattern="redot.$scons_platform.''${target}"

        if [ "$dev_build" = "yes" ]; then
          binary_pattern="$binary_pattern.dev"
        fi

        if [ "$precision" = "double" ]; then
          binary_pattern="$binary_pattern.double"
        fi

        binary_pattern="$binary_pattern.''${target_arch}"

        if [ "$threads" = "no" ]; then
          binary_pattern="$binary_pattern.nothreads"
        fi

        if [ -n "$extra_suffix" ]; then
          binary_pattern="$binary_pattern.$extra_suffix"
        fi

        binary_path="./bin/$binary_pattern"

        if [ ! -f "$binary_path" ]; then
          echo "Building Redot..."
          scons "''${scons_args[@]}" platform=$scons_platform
        fi

        if [ ! -f "$binary_path" ]; then
          echo "Could not find a built Redot binary at $binary_path" >&2
          exit 1
        fi

        exec "$binary_path" "''${runtime_args[@]}"
      '';
    in {
      default = {
        type = "app";
        program = "${script}";
      };
    });

    devShells = forEachSupportedSystem ({
      pkgs,
      deps,
      libraryPathVar,
      ...
    }: {
      default =
        pkgs.mkShell.override
        {
          # Override stdenv in order to change compiler:
          # stdenv = pkgs.clangStdenv;
        }
        {
          packages = deps;
          ${libraryPathVar} = pkgs.lib.makeLibraryPath deps;
        };
    });
  };
}
