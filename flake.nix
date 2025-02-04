{
  description = "Redot Game Engine";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    {
      self,
      nixpkgs,
    }:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      forEachSupportedSystem =
        f:
        nixpkgs.lib.genAttrs supportedSystems (
          system:
          let
            pkgs = import nixpkgs {
              inherit system;
              overlays = [
                self.overlays.default
              ];
            };
          in
          f {
            inherit pkgs system;
          }
        );
    in
    {
      packages = forEachSupportedSystem (
        {
          pkgs,
          system,
          ...
        }:
        {
          default = self.packages.${system}.redot;

          inherit (pkgs)
            redot
            redot-export-templates
            redot-export-templates-linux-debug
            redot-export-templates-linux-release
            redot-export-templates-windows-debug
            redot-export-templates-windows-release
            ;
        }
      );

      overlays.default = final: prev: {
        mkExportTemplate = final.callPackage (import ./export-template-package.nix) { };

        redot = final.callPackage (import ./package.nix) {
          src = self;
          commitHash = if self ? rev then self.rev else "devel";
        };

        redot-export-templates-linux-release = final.mkExportTemplate {
          templateType = "release";
          platform = "linuxbsd";
        };

        redot-export-templates-linux-debug = final.mkExportTemplate {
          templateType = "debug";
          platform = "linuxbsd";
        };

        redot-export-templates-windows-release = final.mkExportTemplate {
          templateType = "release";
          platform = "windows";
        };

        redot-export-templates-windows-debug = final.mkExportTemplate {
          templateType = "debug";
          platform = "windows";
        };

        redot-export-templates = final.symlinkJoin {
          name = "redot-export-templates";
          paths = builtins.attrValues {
            inherit (final)
              redot-export-templates-linux-debug
              redot-export-templates-linux-release
              redot-export-templates-windows-debug
              redot-export-templates-windows-release
              ;
          };
        };
      };

      devShells = forEachSupportedSystem (
        { pkgs, system, ... }:
        {
          default = pkgs.mkShell {
            inputsFrom = [
              self.packages.${system}.redot
            ];

            packages = self.packages.${system}.redot.runtimeDependencies;
          };
        }
      );
    };
}
