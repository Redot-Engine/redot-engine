{
  description = "A Nix-flake-based C/C++ development environment";

  inputs.nixpkgs.url = "https://flakehub.com/f/NixOS/nixpkgs/0.1.*.tar.gz";

  outputs = {
    self,
    nixpkgs,
  }: let
    supportedSystems = ["x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin"];
    forEachSupportedSystem = f:
      nixpkgs.lib.genAttrs supportedSystems (system:
        f rec {
          pkgs = import nixpkgs {inherit system;};
          deps = with pkgs; [
            pkg-config
            autoPatchelfHook
            installShellFiles
            python3
            speechd
            wayland-scanner
            makeWrapper
            mono
            dotnet-sdk_8
            dotnet-runtime_8
            vulkan-loader
            libGL
            xorg.libX11
            xorg.libXcursor
            xorg.libXinerama
            xorg.libXext
            xorg.libXrandr
            xorg.libXrender
            xorg.libXi
            xorg.libXfixes
            libxkbcommon
            alsa-lib
            mono
            wayland-scanner
            wayland
            libdecor
            libpulseaudio
            dbus
            dbus.lib
            speechd
            fontconfig
            fontconfig.lib
            udev
            dotnet-sdk_8
            dotnet-runtime_8
            scons
          ];
        });
  in {
    devShells = forEachSupportedSystem ({
      pkgs,
      deps,
    }: {
      default =
        pkgs.mkShell.override
        {
          # Override stdenv in order to change compiler:
          # stdenv = pkgs.clangStdenv;
        }
        {
          packages = deps;
          LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath deps;
        };
    });
  };
}
