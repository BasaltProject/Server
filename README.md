<p align="center">
  <img src="docs/assets/basalt-logo.png" alt="Basalt" width="360">
</p>

<p align="center">
  <strong>A Minecraft: Java Edition server rewritten from scratch in Kotlin/Native. No JVM.</strong>
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

> ⚠️ **Early development.** Basalt is not usable yet and is not ready for any
> server, public or private. The roadmap below is the honest state of the project.

## Overview

Basalt is a complete reimplementation of the Minecraft: Java Edition server,
written entirely in **Kotlin/Native** and shipped as a native binary for each
platform. There is no JVM anywhere in the runtime: no bundled JRE, no embedded VM,
no hidden Java process.

Two goals drive every decision:

- **Vanilla parity.** A Basalt server with no plugins must behave exactly like
  the official one. Same seed, same world. Same drops, same damage, same redstone,
  same mobs, bugs included. Parity is the acceptance criterion for every system,
  not a long-term aspiration.
- **A plugin platform worth switching to.** **BSMod** is Basalt's native plugin
  API, written in Lua, with first-class tooling: a CLI, a VS Code extension and an
  IntelliJ IDEA plugin.

| | |
|---|---|
| Language | Kotlin/Native, 100 %. No Java, no JVM at runtime |
| Target version | Minecraft 26.2 (protocol 776) |
| Distribution | Native binaries per platform |
| Plugin API | BSMod: Lua, `.bsmod` packages |
| Message model | Adventure-style immutable components; no legacy `§` formatting |
| Configuration | A single `basalt.toml` |
| License | GPL-3.0 |

## Why native

A Minecraft server spends its life doing the same three things: moving bytes,
ticking a world, and waiting. None of that needs a virtual machine.

| | |
|---|---|
| **Startup** | The binary starts immediately. No JVM boot, no class loading, no JIT warm-up |
| **Memory** | No heap sizing, no `-Xmx` tuning, no GC pauses to hide behind |
| **Deployment** | One binary, one config file. No Java version to install, match, or debug |
| **Predictability** | Performance is the same on tick 1 and on tick 1,000,000 |

## Feature scope

| Area | What it means |
|---|---|
| **Vanilla parity** | Bit-exact RNG, identical tick ordering, seed-identical world generation, vanilla datapack execution |
| **World storage** | Linear region format by default (smaller, fewer disk I/O ops than Anvil); existing Anvil (`.mca`) worlds are detected and converted automatically on load |
| **BSMod plugins** | Lua API, `.bsmod` package format, hot reload, sandboxed execution, dependency resolution |
| **Tooling** | Official CLI, VS Code extension, IntelliJ IDEA plugin, generated API docs and type definitions |
| **Legacy clients** | Multi-version support so older Java clients can connect, translated at the edge |
| **Bedrock clients** | Bedrock protocol translation, so Bedrock players join the same server |
| **Bukkit migration** | A compatibility path for the existing ecosystem. Post-1.0, and never at the cost of the no-JVM rule |

## Plugins: BSMod

BSMod is the API. Plugins are written in Lua, packaged as `.bsmod` files
containing a manifest, code, resources and metadata, and run inside a sandbox with
a per-tick instruction budget, so a broken plugin cannot take the server down.

The Lua runtime is [BlueLuaK](https://github.com/BluevaDevelopment/BlueLuaK), a
Kotlin Multiplatform Lua implementation, which means the same runtime everywhere
Basalt runs, with no native Lua dependency to install.

```lua
-- hello.lua
basalt.on("player_join", function(event)
    event.player:send("<gradient:#00c9ff:#92fe9d>Welcome to Basalt!</gradient>")
end)
```

Text is authored in MiniMessage and rendered through the component model, never
through legacy colour codes.

## Configuration

Everything the server exposes lives in a single **`basalt.toml`**, generated
with comments and vanilla defaults on first start. No `server.properties`, no
`bukkit.yml`, no `spigot.yml`, no `paper-global.yml`, no `config/` directory to
hunt through.

```toml
[server]
motd = "A Basalt Server"
max-players = 20

[network]
port = 25565
compression-threshold = 256

[world.overworld]
seed = 0
view-distance = 10
```

Anything that makes the server behave differently from vanilla is opt-in and
labelled as such in the generated comments. If a `server.properties` is present on
first start, its values are imported once.

## Game assets and the EULA

Basalt does **not** contain or distribute any proprietary Minecraft content.
No structures, no loot tables, no datapacks, no textures, no client or server jars.

Once you accept the [Minecraft EULA](https://www.minecraft.net/en-us/eula), the
server downloads the official client jar directly from Mojang, verifies its
checksum, and extracts what it needs into a local cache:

1. `https://piston-meta.mojang.com/mc/game/version_manifest_v2.json`
2. `https://piston-meta.mojang.com/v1/packages/{sha1}/{version}.json`
3. `https://piston-data.mojang.com/v1/objects/{sha1}/client.jar`

Assets come from the official source, never from this repository.

### Accepting the EULA

Same mechanism as the official server, and **the server never prompts you**. On
first start it writes `eula.txt` with `eula=false` and stops. Set it to `eula=true`
and start again.

For non-interactive environments such as Docker or `systemd`, pass the flag
instead:

```bash
./basalt --accept-eula
```

Passing it is a deliberate act and counts as accepting the EULA.

## Building

Gradle orchestrates the build; it is not part of the runtime.

```bash
# Native binary for the current host
./gradlew :basalt-server:linkReleaseExecutableNative

# Debug build and run
./gradlew :basalt-server:runDebugExecutableNative
```

Supported targets: `macosArm64`, `macosX64`, `linuxX64`, `linuxArm64`, `mingwX64`.

## Testing

```bash
# Every suite available on the current host
./gradlew allTests

# A single module
./gradlew :basalt-protocol:nativeTest
```

Native tests only run on their matching host. Beyond unit tests, parity is checked
by replaying scripted scenarios against an official server and diffing the result.

## Requirements

| Component | Requirement |
|---|---|
| JDK | 25, for Gradle only. Not needed to run the server |
| Kotlin | 2.4.10 |
| Gradle | Through the included wrapper |
| Native toolchain | Required to link or run Kotlin/Native binaries on the host |

Use the included wrapper rather than a system Gradle installation.

## Development Status

| Area | Status |
|---|---|
| Build and project layout | In progress |
| Core platform (sockets, buffers, compression, crypto) | Not started |
| NBT and component model | Not started |
| Vanilla RNG parity | Not started |
| Protocol and login | Not started |
| World, entities and gameplay parity | Not started |
| Asset downloader and datapack engine | Not started |
| BSMod and Lua integration | Not started |
| Tooling (CLI, VS Code, IntelliJ) | Not started |
| Legacy client and Bedrock support | Not started |

## Roadmap

1. Build system, module layout and cross-platform CI.
2. Base platform: networking, buffers, compression, encryption, NBT, components,
   and bit-exact vanilla RNG.
3. Protocol, handshake and login.
4. First playable slice: join a world, move, chat.
5. Official assets, EULA flow and vanilla datapack execution.
6. World generation and entities, with seed-identical output.
7. Gameplay parity, system by system.
8. Performance and scheduler design.
9. BSMod, the plugin platform.
10. Legacy client and Bedrock support.
11. Tooling and ecosystem.

## License

Basalt is distributed under the [GNU General Public License v3.0](LICENSE).

Basalt is not affiliated with Mojang Studios or Microsoft. Minecraft is a
trademark of Mojang Studios.
