#pragma once

#include <basalt/wire/common/logging/Logger.hpp>

#include <expected>
#include <string>
#include <vector>

namespace basalt::server::runtime::bukkit {
struct BukkitRuntimeFiles {
    std::string libjvm;
    std::string bootClasspath;
    std::string runtimeJar;
    std::vector<std::string> libraries;
};

/// Unpacks the embedded runtime and fetches the libraries it lists, or says in one message what failed.
[[nodiscard]] std::expected<BukkitRuntimeFiles, std::string> locateRuntime(const wire::common::Logger& logger);
} // namespace basalt::server::runtime::bukkit
