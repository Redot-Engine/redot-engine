{
  pkgsCross,
  symlinkJoin,
  lib,
  redot,
}:
{
  templateType ? "release",
  platform ? "linuxbsd",
}:
let
  exportPlatformNames = {
    linuxbsd = "linux";
    windows = "windows";
  };

  mkTemplate =
    {
      pkg,
      templateType ? "release",
      platform ? "linuxbsd",
      arch ? "x86_64",
    }:
    let
      basePkg =
        (pkg.override {
          withTarget = "template_${templateType}";
          withPlatform = platform;
        }).overrideAttrs
          (old: {
            pname = "redot4-export-templates-${platform}-${templateType}";

            outputs = [ "out" ];

            installPhase = ''
              runHook preInstall

              mkdir -p "$out/share/redot/export_templates/${old.redotVersion}"

              cp bin/redot.* "$out/share/redot/export_templates/${old.redotVersion}/${
                exportPlatformNames.${platform}
              }_${templateType}_${arch}"

              runHook postInstall
            '';
          });
    in
    (
      if platform == "windows" then
        mkWindowsTemplate {
          inherit templateType;
          pkg = basePkg;
        }
      else
        basePkg
    );

  mkWindowsTemplate =
    {
      pkg,
      templateType ? "release",
    }:
    let
      arch = "x86_64";
    in
    (pkg.override {
      withPlatform = "windows";
      inherit arch;
      importEnvVars = [
        "CPLUS_INCLUDE_PATH"
      ];
    }).overrideAttrs
      (
        old:
        let
          buildInputs = (old.buildInputs or [ ]) ++ [
            pkgsCross.mingwW64.windows.mingw_w64_pthreads
            pkgsCross.mingwW64.windows.mcfgthreads
            pkgsCross.mingw32.windows.mcfgthreads
          ];

          libs = symlinkJoin {
            name = "redot-export-templates-windows-libpath";
            paths = buildInputs;
          };
        in
        {
          nativeBuildInputs = old.nativeBuildInputs ++ [
            pkgsCross.mingwW64.buildPackages.gcc
            pkgsCross.mingw32.buildPackages.gcc
          ];

          inherit buildInputs;

          env = (old.env or { }) // {
            CPLUS_INCLUDE_PATH = lib.makeIncludePath buildInputs;
            NIX_LIBS = "${libs}/lib";
          };

          installPhase = ''
            runHook preInstall

            mkdir -p "$out/share/redot/export_templates/${old.redotVersion}"

            cp bin/redot.*.console.exe "$out/share/redot/export_templates/${old.redotVersion}/windows_${templateType}_${arch}_console.exe"
            cp bin/redot.*.${arch}.exe "$out/share/redot/export_templates/${old.redotVersion}/windows_${templateType}_${arch}.exe"

            runHook postInstall
          '';
        }
      );
in
mkTemplate {
  inherit platform templateType;
  pkg = redot;
}
