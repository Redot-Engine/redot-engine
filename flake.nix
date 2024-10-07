{
  description = "A Nix-flake-based C/C++ development environment";

  inputs = {
    nixpkgs.url = "https://flakehub.com/f/NixOS/nixpkgs/0.1.*.tar.gz";
    systems.url = "github:nix-systems/default";
  };

  outputs =
    {
      self,
      nixpkgs,
      systems,
    }:
    let
      forEachSupportedSystem =
        f:
        nixpkgs.lib.genAttrs (import systems) (
          system:
          f {
            pkgs = import nixpkgs { inherit system; };
          }
        );
    in
    {
      packages = forEachSupportedSystem (
        {
          pkgs,
        }:
        {
          default =
            pkgs.callPackage ./package.nix
              {
              };
        }
      );
      devShells = forEachSupportedSystem (
        {
          pkgs,
        }:
        {
          default =
            pkgs.mkShell.override
              {
                # Override stdenv in order to change compiler:
                # stdenv = pkgs.clangStdenv;
              }
              rec {
                packages =
                  with pkgs;
                  [
                    pkgconf
                    autoPatchelfHook
                    installShellFiles
                    python3
                    makeWrapper
                    dotnet-sdk_8
                    dotnet-runtime_8
                    vulkan-loader
                    vulkan-headers
                    fontconfig
                    fontconfig.lib
                    scons
                  ]
                  ++ pkgs.lib.optionals pkgs.stdenv.targetPlatform.isLinux [
                    speechd
                    alsa-lib
                    xorg.libX11
                    xorg.libXcursor
                    xorg.libXinerama
                    xorg.libXext
                    xorg.libXrandr
                    xorg.libXrender
                    xorg.libXi
                    xorg.libXfixes
                    libxkbcommon
                    dbus
                    dbus.lib
                    udev
                    wayland-scanner
                    wayland
                    libdecor
                    libpulseaudio
                  ];
                LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath packages;
              };
        }
      );
    };
}
