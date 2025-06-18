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
        "aarch64-darwing"
      ];

      forEachSupportedSystem =
        f:
        nixpkgs.lib.genAttrs supportedSystems (
          system:
          let
            pkgs = import nixpkgs {
              inherit system;
            };

            mkExportTemplate = pkgs.callPackage (import ./export-template-package.nix) {
              inherit (self.packages.${system}) redot;
            };
          in
          f {
            inherit pkgs mkExportTemplate system;
          }
        );
    in
    {
      packages = forEachSupportedSystem (
        {
          pkgs,
          system,
          mkExportTemplate,
          ...
        }:
        {
          default = self.packages.${system}.redot;

          redot = pkgs.callPackage (import ./package.nix) {
            src = self;
            commitHash = if self ? rev then self.rev else "devel";
          };

          export-templates-linux-release = mkExportTemplate {
            templateType = "release";
            platform = "linuxbsd";
          };

          export-templates-linux-debug = mkExportTemplate {
            templateType = "debug";
            platform = "linuxbsd";
          };

          export-templates-windows-release = mkExportTemplate {
            templateType = "release";
            platform = "windows";
          };

          export-templates-windows-debug = mkExportTemplate {
            templateType = "debug";
            platform = "windows";
          };

          export-templates = pkgs.symlinkJoin {
            name = "redot-export-templates";
            paths = builtins.attrValues {
              inherit (self.packages.${system})
                export-templates-linux-release
                export-templates-linux-debug
                export-templates-windows-release
                export-templates-windows-debug
                ;
            };
          };
        }
      );

      devShells = forEachSupportedSystem (
        { pkgs, system, ... }:
        {
          default = pkgs.mkShell {
            inherit (self.packages.${system}.redot) nativeBuildInputs;

            buildInputs =
              self.packages.${system}.redot.buildInputs ++ self.packages.${system}.redot.runtimeDependencies;
          };
        }
      );
    };
}
