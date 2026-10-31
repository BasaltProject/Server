# Contributing to Basalt Server

Thank you for helping improve Basalt Server. Code, reproducible bug reports, tests and
public documentation improvements are all welcome. Please follow the
[Code of Conduct](.github/CODE_OF_CONDUCT.md).

## Start in the right place

- Check existing issues and pull requests before opening a duplicate.
- Use the [issue forms](https://github.com/BasaltProject/Server/issues/new/choose) for bugs and feature requests.
  Include exact versions, reproducible steps and redacted diagnostics.
- Discuss large changes, new dependencies, public API changes or incompatible
  behavior in an issue before investing in an implementation.
- Send suspected vulnerabilities through [SECURITY.md](SECURITY.md), never a
  public issue or pull request containing exploit details.
- This repository is for Basalt Server. The other product lives in
  [Basalt Proxy](https://github.com/BasaltProject/Proxy). Shared network,
  protocol and utility fixes may belong in [Wire](https://github.com/BasaltProject/Wire).

## Build and test

Use CMake 3.28 or newer, Ninja and a Clang toolchain with C++23 support. See the
[README](README.md) and [CI workflow](.github/workflows/build.yml) for current build
requirements and platforms. Clone recursively, or initialize the pinned submodules:

```sh
git submodule update --init --recursive
cmake --preset debug
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

The release configuration is `cmake --preset release` followed by
`cmake --build build/release`. Binaries are written to `build/release/bin/`.
For memory-safety changes, also run the sanitizer configuration where supported:

```sh
cmake --preset asan
cmake --build build/asan
ctest --test-dir build/asan --output-on-failure
```

Changes to `server-bukkit` also need the compatibility layer enabled and its tests
run. Building that layer on supported targets requires a JDK 25 with JNI headers,
`javac` and `jar`; set `JAVA_HOME` when discovery needs help. The AUTO configuration
can omit it if the toolchain is unavailable, so check the configure output rather
than assuming those tests ran. This is a build dependency, not an external JVM
requirement for a deployed server.

Run focused tests while iterating and the relevant suite before submitting. List
actual commands, results and tested platforms in the pull request; say explicitly
when a check was not run. Documentation-only changes do not require a native build.

## Server-specific expectations

- Preserve vanilla behavior unless a deliberate, documented opt-in changes it.
  For gameplay, world generation or storage changes, identify the Minecraft version
  and demonstrate the expected behavior with reproducible evidence.
- Keep worlds safe: use disposable saves and synthetic fixtures. Describe any
  world-format, migration, save/load or generation impact; never test against a
  production world without a backup.
- Fix Bukkit/Spigot/Paper compatibility at the shared API or runtime layer. Do not
  add branches for a named plugin, modify its jar, or hide an exception in place of
  implementing the general contract. Include a generic reproducer.
- Keep Java compatibility in its own module. The native core must still work
  without Bukkit plugins; do not introduce an external JVM requirement for operators.

## Code and tests

- Keep changes focused and match nearby C++23 code. Use the repository's
  `.clang-format`; avoid reformatting unrelated files.
- Prefer small modules and clear ownership. Do not assume plugin callbacks,
  network work or scheduled tasks always run on one thread.
- Keep text in the component model; legacy formatting belongs at explicit
  compatibility boundaries, not in new internal representations or APIs.
- Add regression coverage for behavior changes and meaningful shared contracts.
  Native tests use doctest; follow the existing module's test structure.
- Use portable code and consider the CI targets. Document a new dependency's
  purpose, source, license and platform impact before adding it.
- The main implementation areas include server-runtime, server-world, server-plugins and server-bukkit.
  If a change belongs in a shared submodule, submit it there and link that change
  from the consuming pull request. Update only the necessary pinned revisions.

## Pull request checklist

Create a topic branch from current `main`, keep unrelated changes out, and fill
in the [pull request template](.github/PULL_REQUEST_TEMPLATE.md). Explain the
problem, solution, compatibility effects and how the result was verified. Link
related issues and shared-component pull requests.

Use concise, single-line Conventional Commit subjects in English, for example
`fix(world): retain worlds during tick callbacks`.
Be ready to revise the implementation during review. Maintainers may request
changes or decline a proposal that conflicts with project scope or architecture.

## Safe, reviewable contributions

Only submit work you have the right to contribute under the repository's
[license](LICENSE). Do not copy proprietary implementation code or commit Minecraft
jars/assets, third-party plugin jars, real player data, private worlds, credentials,
license keys or unredacted logs. Use synthetic fixtures and minimal examples.

Keep local investigation notes and machine-specific paths out of the pull request.
Public operator and plugin documentation is maintained at
[basaltmc.org/docs](https://basaltmc.org/docs/server/introduction).
