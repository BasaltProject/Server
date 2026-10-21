#pragma once

#include <basalt/server/runtime/config/ConfigDirectory.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace basalt::server::runtime::config {
struct BukkitConfig {
    static constexpr std::string_view kFileName = "bukkit.toml";

    enum class Mode : std::uint8_t { Auto, On, Off };

    Mode mode = Mode::Auto;
    bool download = true;
    std::string runtime;
    std::string libjvm;
    std::string bootClasspath;
    std::string runtimeJar;
    std::string libraries;

    [[nodiscard]] static std::expected<BukkitConfig, std::string> load(const ConfigDirectory& directory);
};
} // namespace basalt::server::runtime::config
