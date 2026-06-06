#pragma once

#include <basalt/server/runtime/config/BukkitConfig.hpp>
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

/// Where to look for the runtime, best first.
[[nodiscard]] std::vector<std::string> runtimeRoots(const config::BukkitConfig& config);

/// Finds every piece of the runtime, fetching missing libraries, or says in one message what to do.
[[nodiscard]] std::expected<BukkitRuntimeFiles, std::string> locateRuntime(const config::BukkitConfig& config,
                                                                           const wire::common::Logger& logger);
} // namespace basalt::server::runtime::bukkit
