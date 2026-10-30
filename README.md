<p align="center">
  <a href="https://raw.githubusercontent.com/BasaltProject/.github/main/assets/basalt-logo-white.svg#gh-dark-mode-only">
    <img src="https://raw.githubusercontent.com/BasaltProject/.github/main/assets/basalt-logo-white.svg" alt="Basalt" width="360">
  </a>
  <a href="https://raw.githubusercontent.com/BasaltProject/.github/main/assets/basalt-logo-black.svg#gh-light-mode-only">
    <img src="https://raw.githubusercontent.com/BasaltProject/.github/main/assets/basalt-logo-black.svg" alt="Basalt" width="360">
  </a>
</p>

<h2 align="center">Basalt Server</h2>

<p align="center">
  <strong>A Minecraft server rewritten from scratch in C++23. No external JVM.</strong>
</p>

<p align="center">
  <img alt="Minecraft" src="https://img.shields.io/badge/Minecraft-26.3-62B47A?logo=minecraft&logoColor=white">
  <img alt="Protocol" src="https://img.shields.io/badge/protocol-777-62B47A">
  <img alt="C++" src="https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white">
  <img alt="Platforms" src="https://img.shields.io/badge/platforms-macOS_%7C_Linux_%7C_Windows-00599C">
  <img alt="Build" src="https://img.shields.io/badge/build-CMake-064F8C?logo=cmake&logoColor=white">
  <img alt="Plugins" src="https://img.shields.io/badge/plugins-Lua-000080?logo=lua&logoColor=white">
  <img alt="License" src="https://img.shields.io/badge/license-GPL--3.0-green">
</p>

> ⚠️ **Early development.** Most of vanilla's gameplay is in place and Java plugins
> run, but some systems are still incomplete and the server has seen little
> long-term use. Back up your worlds, and expect rough edges before trusting it
> with a public server.

## Overview

Basalt Server is a complete reimplementation of the Minecraft server, written
entirely in **C++23** and shipped as a native binary for each platform.
The native core requires no external JVM.

It speaks Java Edition's protocol and uses its world format. Older Java clients and
Bedrock clients are served through translation layers at the edge, which are still
maturing.

Bukkit, Spigot and Paper plugins also run. Server implements those APIs and the
plugin lifecycle in its own module, and [Shim](https://github.com/BasaltProject/Shim),
a JVM implemented in C++ (a fork of Avian aimed at Java 25), executes the bytecode
inside the server process. Shim is built into the binary, so no Java installation
is needed, and it only starts when a jar is present in `plugins-bukkit/`.
Large plugins such as FastAsyncWorldEdit and PacketEvents are part of the test
suite. Native Lua plugins remain the first-class path and the one the API is
designed around.

Two goals drive every decision:

- **Vanilla parity.** A Basalt Server with no plugins must behave exactly like the
  official one. Same seed, same world. Same drops, same damage, same redstone,
  same mobs, bugs included. Parity is the acceptance criterion for every system,
  not a long-term aspiration.
- **A plugin platform worth switching to.** The plugin API is native and written
  in Lua, with first-class tooling: a CLI, a VS Code extension and an IntelliJ
  IDEA plugin.

| | |
|---|---|
| Language | C++23 native core; no external JVM required |
| Target version | Minecraft 26.3 (protocol 777) |
| Distribution | Native binaries per platform |
| World storage | Linear regions by default; Anvil worlds are read and converted on load |
| Plugin API | Lua, `.bspkg` packages; Bukkit, Spigot and Paper jars through Shim |
| Message model | Adventure-style immutable components; no legacy `§` formatting |
| Configuration | A `config/` folder, one TOML file per domain |
| Remote management | REST and WebSocket API with token auth; RCON only for compatibility, off by default |
| License | GPL-3.0 |

## Why native

A Minecraft server spends its life moving bytes, ticking a world and waiting.
None of that needs a virtual machine.

The core starts without a VM boot, class loading or warm-up. There is no heap to
size and no collector to hide behind. Deployment is one binary and a config folder,
with no runtime to install or match. Tick 1 and tick 1,000,000 cost the same. The
embedded Java runtime only comes up when Bukkit-style jars are present, and a server
without them never pays for it.

## Plugins

Plugins are written in Lua and packaged as `.bspkg` files, each with a manifest,
code, resources and metadata. They run inside a sandbox with a per-tick
instruction budget, so a broken plugin cannot take the server down.

```lua
-- hello.lua
basalt.on("player_join", function(event)
    event.player:send("<gradient:aqua:green>Welcome to Basalt!</gradient>")
end)
```

Text is authored in MiniMessage and rendered through the component model, never
through legacy colour codes.

The Lua runtime is the official Lua 5.5.1, built from source into the binary, so
it is the same runtime everywhere Basalt runs with nothing to install alongside
it.

## Configuration

Configuration lives in **`config/`**, one TOML file per domain, generated with
comments and vanilla defaults on first start:

```
config/
  server.toml        network, remote API, plugins, performance, logging
  worlds.toml        worlds to load and the defaults every world inherits
  groups.toml        permission groups
  limits.toml        per-connection packet and spam limits
  server_list.toml   what the server list shows
  commands.toml      per-command settings
  messages.toml      server messages
  tab_list.toml      scoreboard.toml  boss_bar.toml  animations.toml
```

Anything that makes the server behave differently from vanilla is opt-in and
labelled as such in the generated comments. World rules, mob settings and gameplay options
are set per world, inheriting from the defaults in `worlds.toml` and overridable in
`worlds/<name>/`. A `server.properties` found on first start is imported once.

## Game assets and the EULA

Basalt Server does **not** contain or distribute any proprietary Minecraft
content: no structures, no loot tables, no datapacks, no textures, no jars.

Once you accept the [Minecraft EULA](https://www.minecraft.net/en-us/eula), the
server downloads the official server download from Mojang, verifies its SHA-1 and
unpacks only the data it needs into a local cache. That happens once per version;
later starts touch neither the network nor the archive.

**The server never prompts you.** On first start it writes `eula.toml` with
`eula = false` and stops. Set it to `eula = true` and start again. An accepted
`eula.txt` from an existing server is read once and migrated. For non-interactive
environments, `./basalt --accept-eula` is a deliberate act and counts as
accepting it.

## Building

The network layer lives in [Wire](https://github.com/BasaltProject/Wire), shared
with the proxy and pulled in as a submodule, so clone with it:

```bash
git clone --recurse-submodules https://github.com/BasaltProject/Server.git
```

In an existing clone, `git submodule update --init` does the same. Without it the
build fails immediately: half the modules are missing.

You need CMake 3.28 or newer, Ninja and Clang. Then two commands, the same on
macOS, Linux and Windows:

```bash
cmake --preset release
cmake --build build/release
```

That leaves a release binary in `build/release/bin/`, named for what it is:

```
build/release/bin/BasaltServer-26.3.0-dev-macOS-arm64
build/release/bin/BasaltServer-26.3.14-Linux-x64
build/release/bin/BasaltServer-26.3.14-Windows-x64.exe
```

A build made outside the CI has no build number, so it is `.0-dev`.

The tests run with `ctest --preset debug`, and `--preset asan` builds the same
tree under the address and undefined sanitizers, which is what the CI does on
every push.

## License

Basalt Server is distributed under the [GNU General Public License v3.0](LICENSE).

Basalt is not affiliated with Mojang Studios or Microsoft. Minecraft is a
trademark of Mojang Studios.
