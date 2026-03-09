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

## Manual builds with `nix develop`

For full manual control over the build process:

```bash
# Enter the Nix development environment.
nix develop

# Build Redot (use 'macos' on macOS, 'linuxbsd' on Linux).
scons platform=linuxbsd  # or: scons platform=macos

# Example with extra build flags.
scons platform=linuxbsd target=template_release production=yes

# Run the editor - binary name reflects your platform and architecture.
# Examples: redot.linuxbsd.editor.x86_64, redot.macos.editor.arm64
./bin/redot.<platform>.editor.<arch>
```

## Notes

- `nix run .` only auto-builds when a matching binary does not exist yet.
- If you want to rebuild with different flags after a binary was already produced, remove the existing binary first or use `nix develop` and run `scons` manually.
- The flake app automatically detects your platform and architecture.
- Nix works on Linux and macOS, and is available at [nixos.org/download.html](https://nixos.org/download.html).
