# Redot Engine LTS — NVIDIA PhysX Edition

<p align="center">
  <a href="https://redotengine.org/">
    <img src="logo_outlined.png" width="400" alt="Redot Engine logo">
  </a>
</p>

**This repository is the home of the Redot PhysX project: the Redot Engine
LTS with the NVIDIA PhysX 5 physics module fully integrated** — GPU
rigid-body dynamics, GPU particles and fluids (with whitewater foam), cloth,
gas, vehicles, water, Blast destruction and NVIDIA Flow — for Windows and
Linux.

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Redot-Engine/redot-engine)

## 2D and 3D cross-platform game engine

**[Redot Engine](https://redotengine.org) is a feature-packed, cross-platform
game engine to create 2D and 3D games from a unified interface.** It provides a
comprehensive set of common tools, so that
users can focus on making games without having to reinvent the wheel. Games can
be exported with one click to a number of platforms, including the major desktop
platforms (Linux, macOS, Windows), mobile platforms (Android, iOS), as well as
Web-based platforms and consoles.

## Free, open source and community-driven

Redot is a completely free and open source fork of Godot under the very permissive MIT license.
No strings attached, no royalties, nothing. The users' games are theirs, down
to the last line of engine code. Redot's development is fully independent and truly
community-driven, empowering users to help shape their engine to match their
expectations.

Before being open sourced in [February 2014](https://github.com/godotengine/godot/commit/0b806ee0fc9097fa7bda7ac0109191c9c5e0a1ac),
Godot had been developed by [Juan Linietsky](https://github.com/reduz) and
[Ariel Manzur](https://github.com/punto-) (both still maintaining Godot)
for several years as an in-house engine, used to publish several work-for-hire
titles.

Redot was forked from Godot in [September 2024](https://github.com/Redot-Engine/redot-engine/commit/a12e9de5dd831e1ce0c839f0420b278ef0a6aa5b),
intending to improve upon Godot in order to fulfill its potential and contribute to the shared
codebase of both through a more genuinely community-driven model than Godot.

[Kaetram - 2D Pixel Cross-Platform MMORPG by Keros](https://kaetram.com)
<p align="center">
	<img src="screenshot.jpg" width="900" alt="Redot Engine screenshot!">
</p>

## Getting the engine

### Binary downloads

PhysX-enabled editor and export-template builds are published on this
repository's [Releases page](https://github.com/Vlado-VG/redotplusplus/releases).
Windows binaries will be code-signed through the
[SignPath Foundation](https://signpath.org/) in future builds. Checksums for every artifact
are published alongside the downloads.

Vanilla Redot binaries (without the PhysX module) can be found
[on the Redot website](https://redotengine.org/download).

### Compiling from source

[See the official docs](https://docs.redotengine.org/en/Contributing/Development/compiling/getting_source)
for compilation instructions for every supported platform.

#### Using Nix (recommended)

If you have the Nix package manager installed, you can build and run the editor in one command:

```bash
nix run .
```

This will automatically install all build dependencies and compile Redot if the binary doesn't exist.

Detailed Nix usage, including passing SCons build flags through `nix run`, forwarding runtime arguments, and manual `nix develop` workflows, is documented in the `Nix usage guide` at `doc/nix.md`.

#### PhysX module build flags

The bundled PhysX module ships with prebuilt SDK libraries and accepts
the following SCons flags (defaults first):

| Flag | Values | What it controls |
| --- | --- | --- |
| `physx_config` | `checked`, `release` | Which prebuilt SDK library set is linked. `checked` (default) pairs with editor and `target=template_debug` builds; `release` pairs with `target=template_release`. |
| `physx_gpu` | `yes`, `no` | CUDA GPU dynamics (Windows and Linux). With `no`, the engine always simulates on the CPU and never initializes CUDA. |
| `flow` | `yes`, `no` | NVIDIA Flow sparse-grid fluid simulation (volumetric fire, smoke, and fluids). |
| `blast` | `yes`, `no` | NVIDIA Blast mesh destruction. The prebuilt Blast libraries are Windows-only; other platforms build without it. |

Example — a Windows release template with the optimized SDK libraries:

```bash
scons platform=windows target=template_release physx_config=release
```


## Community and Contributing

Redot is not only an engine but an ever-growing community of users and engine
developers. Please visit our [Discord server](https://discord.gg/redot)!

To get started contributing to the project, see the [contributing guide](CONTRIBUTING.md).
This document also includes guidelines for reporting bugs.

Follow [Redot on X/Twitter](https://x.com/Redot_Engine)!
## Credits
**Credits** to [**Uno "Wild-ox"**](https://github.com/uno1982) studios for:
MPM shaders, a lot of fixes and implementations for water, 2W vehicle, boat, foam and more.
Visit his Godot upstream (4.7+) PhysX module at:
https://github.com/uno1982/godot/tree/feature/physx5-module

## Documentation and demos

The class reference is accessible from the Redot editor.

## AI Integration - Model Context Protocol (MCP)

Redot supports AI integration using MCP. See the [setup instructions](doc/mcp-integration.md).
