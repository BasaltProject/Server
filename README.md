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
  <strong>A Minecraft server for Java and Bedrock, rewritten from scratch in C++. No JVM required.</strong>
</p>

<p align="center">
  <img alt="Java Edition" src="https://img.shields.io/badge/Java_Edition-26.3-62B47A?logo=minecraft&logoColor=white">
  <img alt="Bedrock Edition" src="https://img.shields.io/badge/Bedrock_Edition-26.60-62B47A?logo=minecraft&logoColor=white">
  <img alt="C++" src="https://img.shields.io/badge/language-C%2B%2B-00599C?logo=cplusplus&logoColor=white">
  <img alt="Platforms" src="https://img.shields.io/badge/platforms-macOS_%7C_Linux_%7C_Windows_%7C_Android_%7C_FreeBSD-00599C">
  <img alt="License" src="https://img.shields.io/badge/license-GPL--3.0-green">
</p>

> ⚠️ **Early development.** Back up your worlds and expect rough edges before using it on a public server.

## About

Basalt Server is a native Minecraft server that welcomes **Java Edition and Bedrock Edition players on the same
world**, with no proxy or extra plugin in between. It aims for exact vanilla parity: the same seed gives the same
world, with the same drops, damage, redstone and mobs.

| Edition | Version | Connection |
|---|---|---|
| Java Edition | 26.3 | Native |
| Bedrock Edition | 26.60 | Native (NetherNet and RakNet) |

- **Cross-play out of the box.** Java and Bedrock clients join the same server and play together.
- **Native and light.** One binary and a `config/` folder. No JVM to install, no garbage collector pausing the tick.
- **WebAssembly plugins.** The native API, sandboxed and packaged as `.bspkg`, in C++, Zig, Rust or any language that
  compiles to wasm32.
- **Bukkit, Spigot and Paper plugins.** Run through [Shim](https://github.com/BasaltProject/Shim), a JVM built into
  the binary that only starts when a jar is present.

## Getting started

Download a binary from the [releases page](https://github.com/BasaltProject/Server/releases) (stable builds and a
rolling **Nightly**), run it once, accept the [Minecraft EULA](https://www.minecraft.net/en-us/eula) by setting
`eula = true` in `eula.toml`, and start it again. Or pass `--accept-eula`.

Configuration, plugins and everything else are in the [documentation](https://basaltmc.org/docs/server/introduction).

## Building

```bash
git clone --recurse-submodules https://github.com/BasaltProject/Server.git
cd Server
cmake --preset release
cmake --build build/release
```

Needs CMake 3.28+, Ninja and Clang. The binary ends up in `build/release/bin/`.

## Contributing

Read the [contribution guide](CONTRIBUTING.md) and the [Code of Conduct](.github/CODE_OF_CONDUCT.md).
Report vulnerabilities privately through the [security policy](SECURITY.md).

## License

[GPL-3.0](LICENSE). Basalt is not affiliated with Mojang Studios or Microsoft. Minecraft is a trademark of Mojang Studios.
