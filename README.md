<p align="center">
  <img src="docs/assets/basalt-logo.png" alt="Basalt" width="360">
</p>

<h2 align="center">Basalt Server</h2>

<p align="center">
  <strong>A Minecraft server rewritten from scratch in Kotlin/Native. No JVM.</strong>
</p>

<p align="center">
  <img alt="Minecraft" src="https://img.shields.io/badge/Minecraft-26.2-62B47A?logo=minecraft&logoColor=white">
  <img alt="Protocol" src="https://img.shields.io/badge/protocol-776-62B47A">
  <img alt="Kotlin" src="https://img.shields.io/badge/Kotlin-2.4.10-7F52FF?logo=kotlin&logoColor=white">
  <img alt="Kotlin/Native" src="https://img.shields.io/badge/Kotlin%2FNative-macOS_%7C_Linux_%7C_Windows-7F52FF?logo=kotlin&logoColor=white">
  <img alt="Build" src="https://img.shields.io/badge/build-Gradle-02303A?logo=gradle&logoColor=white">
  <img alt="Plugins" src="https://img.shields.io/badge/plugins-Lua-000080?logo=lua&logoColor=white">
  <img alt="License" src="https://img.shields.io/badge/license-GPL--3.0-green">
</p>

> ⚠️ **Early development.** You can connect a client and play with a good part of
> the game, but plenty is missing or wrong. Not ready for any server, public or
> private.

## Overview

Basalt Server is a complete reimplementation of the Minecraft server, written
entirely in **Kotlin/Native** and shipped as a native binary for each platform.
There is no JVM anywhere in the runtime: no bundled JRE, no embedded VM, no hidden
Java process.

It speaks Java Edition's protocol and uses its world format, so that is the client
it serves today. Older Java clients and Bedrock clients are planned, translated at
the edge, and neither is implemented yet.

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
| Language | Kotlin/Native, 100 %. No Java, no JVM at runtime |
| Target version | Minecraft 26.2 (protocol 776) |
| Distribution | Native binaries per platform |
| World storage | Linear regions by default; Anvil worlds are read and converted on load |
| Plugin API | Lua, `.bspkg` packages |
| Message model | Adventure-style immutable components; no legacy `§` formatting |
| Configuration | A `config/` folder, one TOML file per domain |
| License | GPL-3.0 |

## Why native

A Minecraft server spends its life moving bytes, ticking a world and waiting.
None of that needs a virtual machine.

The binary starts immediately: no VM boot, no class loading, no warm-up. There is
no heap to size and no collector to hide behind. Deployment is one binary and a
config folder, with no runtime to install or match. Tick 1 and tick 1,000,000 cost
the same.

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

The Lua runtime is [BlueLuaK](https://github.com/BluevaDevelopment/BlueLuaK), a
Kotlin Multiplatform implementation, so it is the same runtime everywhere Basalt
runs with no native Lua dependency to install.

## Configuration

Configuration lives in **`config/`**, one TOML file per domain, generated with
comments and vanilla defaults on first start:

```
config/
  server.toml        network, logging, performance
  worlds.toml        world defaults, inherited by worlds/<name>/settings.toml
  groups.toml        permission groups
  server_list.toml   what the server list shows
  commands.toml      per-command settings
```

Anything that makes the server behave differently from vanilla is opt-in and
labelled as such in the generated comments. A `server.properties` found on first
start is imported once.

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

Kotlin/Native tasks carry the target in their name; replace `MacosArm64` with the
one you are building for. Gradle orchestrates the build and is not part of the
runtime; it needs JDK 25, which the server itself does not.

```bash
# Native binary for the current host
./gradlew :server-runtime:linkReleaseExecutableMacosArm64

# Debug build and run
./gradlew :server-runtime:runDebugExecutableMacosArm64
```

Supported targets: `macosArm64`, `macosX64`, `linuxX64`, `linuxArm64`, `mingwX64`.
Kotlin/Native does not cross-compile between host families, so each one is built
on its own.

## Related repositories

| Repository | What it is |
|---|---|
| [Wire](https://github.com/BasaltProject/Wire) | The network layer this repository builds on, shared with the proxy |
| [Proxy](https://github.com/BasaltProject/Proxy) | The proxy component |
| [Website](https://github.com/BasaltProject/Website) | The project site, at [basaltmc.org](https://basaltmc.org) |

## License

Basalt Server is distributed under the [GNU General Public License v3.0](LICENSE).

Basalt is not affiliated with Mojang Studios or Microsoft. Minecraft is a
trademark of Mojang Studios.
