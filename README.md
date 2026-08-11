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

> ⚠️ **Early development.** Basalt Server is not ready for any server, public or
> private. The status table below is the honest state of the project.

## Overview

Basalt Server is a complete reimplementation of the Minecraft server, written
entirely in **Kotlin/Native** and shipped as a native binary for each platform.
There is no JVM anywhere in the runtime: no bundled JRE, no embedded VM, no hidden
Java process.

It is the server component of the **Basalt** project.

**Which clients can connect.** The server speaks Java Edition's protocol and uses
its world format, so that is the client it serves today. Older Java clients and
Bedrock clients are both planned, translated at the edge, and neither is
implemented yet. Nothing below claims otherwise.

Two goals drive every decision:

- **Vanilla parity.** A Basalt Server with no plugins must behave exactly like
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

[server]
port = 25565

[world]
view-distance = 10
save-interval-seconds = 30
```

Anything that makes the server behave differently from vanilla is opt-in and
labelled as such in the generated comments. If a `server.properties` is present on
first start, its values are imported once.

## Game assets and the EULA

Basalt Server does **not** contain or distribute any proprietary Minecraft content.
No structures, no loot tables, no datapacks, no textures, no client or server jars.

Once you accept the [Minecraft EULA](https://www.minecraft.net/en-us/eula), the
server downloads the official server download directly from Mojang, verifies its
SHA-1, unpacks only the data it needs into a local cache, and deletes the archive:

1. `https://piston-meta.mojang.com/mc/game/version_manifest_v2.json`
2. `https://piston-meta.mojang.com/v1/packages/{sha1}/{version}.json`
3. `https://piston-data.mojang.com/v1/objects/{sha1}/server.jar`

That happens once per version. Later starts read the unpacked files and touch
neither the network nor the archive. Assets come from the official source, never
from this repository.

### Accepting the EULA

Same mechanism as the official server, and **the server never prompts you**. On
first start it writes `eula.toml` with `eula = false` and stops. Set it to
`eula = true` and start again. An accepted `eula.txt` from an existing server is
read once and migrated, so hosting panels and migrated worlds keep working.

For non-interactive environments such as Docker or `systemd`, pass the flag
instead:

```bash
./basalt --accept-eula
```

Passing it is a deliberate act and counts as accepting the EULA.

## Building

The network layer lives in [Wire](https://github.com/BasaltProject/Wire), shared
with the proxy and pulled in as a submodule, so clone with it:

```bash
git clone --recurse-submodules https://github.com/BasaltProject/Server.git
```

In an existing clone, `git submodule update --init` does the same thing. Without
it the build fails immediately: half the modules are missing.

Gradle orchestrates the build; it is not part of the runtime.

Kotlin/Native tasks carry the target in their name; replace `MacosArm64` with the
one you are building for.

```bash
# Native binary for the current host
./gradlew :basalt-server:linkReleaseExecutableMacosArm64

# Debug build and run
./gradlew :basalt-server:runDebugExecutableMacosArm64
```

Supported targets: `macosArm64`, `macosX64`, `linuxX64`, `linuxArm64`, `mingwX64`.

## Testing

```bash
# Everything CI runs: compile, style and every suite on this host
./gradlew build

# A single module
./gradlew :basalt-protocol:macosArm64Test
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
| Build, module layout and cross-platform CI | Working |
| Core platform (sockets, buffers, zlib/zstd/gzip, crypto) | Working |
| NBT, SNBT and the component model | Working |
| Vanilla RNG parity (LCG, xoroshiro, gaussian, positional) | Working |
| Protocol, handshake, login, configuration and play | Working |
| Online mode: encryption, Mojang session, profiles | Working |
| Official asset download and the EULA flow | Working |
| World storage: Linear regions, with Anvil worlds read on load | Working |
| World generation: density router, surface rules, biomes, decoration | In progress |
| Structures | Plains villages only |
| Entities | Skeleton only: an ECS store and a movement system |
| Gameplay parity (redstone, mobs, combat, inventories) | Not started |
| Datapack execution (loot tables, recipes, advancements) | Not started |
| BSMod and the Lua runtime | Not started |
| Tooling (CLI, VS Code, IntelliJ) | Not started |
| Older Java clients (`basalt-multiversion`) | Not started |
| Bedrock clients (`basalt-bedrock`) | Not started |
| Bukkit compatibility | Out of scope until BSMod is stable |

What that adds up to today: a real client connects, authenticates, spawns in a
generated world, flies around while chunks stream in, breaks blocks, chats, and
sees other players do the same. Everything below that line is still missing.

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
