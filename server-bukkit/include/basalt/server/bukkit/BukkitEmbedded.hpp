#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace basalt::server::bukkit {
/// The Shim machine and the Java half, compiled into the binary. Every piece is empty when the build could not make it.
struct EmbeddedRuntime {
    std::span<const std::uint8_t> libjvm;
    std::span<const std::uint8_t> bootClasspath;
    std::span<const std::uint8_t> runtimeJar;
    std::string_view libraries;

    [[nodiscard]] bool complete() const { return !libjvm.empty() && !bootClasspath.empty() && !runtimeJar.empty(); }
};

[[nodiscard]] EmbeddedRuntime embeddedRuntime();
} // namespace basalt::server::bukkit
