# Nix workflow

If you have the Nix package manager installed, you can build and run the editor in one command:

```bash
nix run .
```

This automatically installs the required build dependencies and compiles Redot if a matching binary does not already exist.

## Passing SCons build flags

You can pass SCons build flags directly through `nix run`:

```bash
# Build with custom SCons flags, then run the editor.
nix run . -- target=editor dev_build=yes num_jobs=12

# Build a release template.
nix run . -- target=template_release production=yes

# Pass SCons build flags first, then `--`, then runtime args for the editor.
nix run . -- target=editor dev_build=yes -- --path /tmp/project
```

Argument handling works like this:

- The first `--` is consumed by Nix.
- Arguments before the next `--` are passed to `scons`.
- Arguments after the next `--` are passed to the built Redot binary.

For a quick reminder of the wrapper syntax, run:

```bash
nix run . -- --help
```

This supports the same SCons flags documented by the build system, such as `production=yes`, `target=template_release`, `module_mono_enabled=yes`, `precision=double`, `ccflags=...`, and more.

The wrapper only uses a subset of those flags when deciding which existing binary can be reused: `target`, `arch`, `dev_build`, `precision`, `threads`, and `extra_suffix`. Other SCons flags such as `production=yes`, `module_mono_enabled=yes`, or custom `ccflags` are still forwarded to `scons`, but they do not change the wrapper's reuse check.

## Manual builds with `nix develop`

For full manual control over the build process:

```bash
# Enter the Nix development environment.
nix develop

# Build Redot (use 'macos' on macOS, 'linuxbsd' on Linux).
scons platform=linuxbsd  # or: scons platform=macos

# Example with extra build flags.
scons platform=linuxbsd target=template_release production=yes

# Run the editor - binary names can include optional suffixes.
# Examples:
#   redot.linuxbsd.editor.x86_64
#   redot.linuxbsd.editor.dev.x86_64
#   redot.linuxbsd.editor.double.x86_64
#   redot.linuxbsd.editor.x86_64.nothreads
./bin/redot.<platform>.<target>[.dev][.double].<arch>[.nothreads][.<extra_suffix>]
```

## Notes

- `nix run .` only auto-builds when the expected binary for the wrapper-managed naming fields does not exist yet.
- If you want to rebuild with different flags after a binary was already produced, remove the existing binary first or use `nix develop` and run `scons` manually.
- The flake app automatically detects your platform and architecture, and `arch=auto` resolves to the host architecture.
- Nix works on Linux and macOS, and is available at [nixos.org/download.html](https://nixos.org/download.html).
