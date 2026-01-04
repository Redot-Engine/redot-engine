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
          binary = if isDarwin then "redot.macos.editor.x86_64" else "redot.linuxbsd.editor.x86_64";
        });
  in {
    apps = forEachSupportedSystem ({
      pkgs,
      deps,
      libraryPathVar,
      platform,
      binary,
    }: let
      script = pkgs.writeShellScript "redot" ''
        export ${libraryPathVar}=${pkgs.lib.makeLibraryPath deps}
        if [ ! -f ./bin/${binary} ]; then
          echo "Building Redot..."
          scons platform=${platform}
        fi
        exec ./bin/${binary} "$@"
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
