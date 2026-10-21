#pragma once

#include <basalt/server/runtime/config/ConfigDirectory.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace basalt::server::runtime::config {
struct BukkitConfig {
    static constexpr std::string_view kFileName = "bukkit.toml";

    bool enabled = false;
    std::string libjvm;
    std::string bootClasspath;
    std::string runtimeJar;
    std::string libraries;
    std::string plugins = "plugins-bukkit";

    [[nodiscard]] static std::expected<BukkitConfig, std::string> load(const ConfigDirectory& directory);
};
} // namespace basalt::server::runtime::config
