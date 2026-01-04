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
            wayland
            libdecor
            libpulseaudio
            dbus
            dbus.lib
            fontconfig
            fontconfig.lib
            udev
            scons
          ];
        });
  in {
    apps = forEachSupportedSystem ({
      pkgs,
      deps,
    }: let
      script = pkgs.writeShellScript "redot" ''
        export LD_LIBRARY_PATH=${pkgs.lib.makeLibraryPath deps}
        if [ ! -f ./bin/redot.linuxbsd.editor.x86_64 ]; then
          echo "Building Redot..."
          scons platform=linuxbsd
        fi
        exec ./bin/redot.linuxbsd.editor.x86_64 "$@"
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
