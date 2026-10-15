#pragma once

#include <basalt/server/runtime/config/ConfigDirectory.hpp>
#include <basalt/wire/auth/forwarding/ProxyForwarding.hpp>
#include <basalt/wire/common/logging/LogLevel.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::config {

enum class ColorMode : std::uint8_t { Auto, Always, Never };

[[nodiscard]] std::string_view colorModeName(ColorMode mode) noexcept;

struct BasaltConfig {
    static constexpr std::string_view kFileName = "server.toml";

    bool javaEnabled = true;
    std::int32_t port = 25565;
    std::string javaBind = "0.0.0.0";
    bool javaDualStack = false;

    bool bedrockEnabled = true;
    std::int32_t bedrockPort = 19132;
    std::string bedrockPrefix = ".";
    std::int32_t maxPlayers = 80;
    bool onlineMode = true;
    /// parity: `enforce-secure-profile`, which only matters in online mode.
    bool enforceSecureProfile = true;
    std::string gamemode = "survival";
    bool whitelist = false;
    wire::auth::forwarding::ProxyForwarding forwarding;
    wire::common::LogLevel logLevel = wire::common::LogLevel::Info;
    bool logToFile = true;
    std::int32_t logKeepDays = 14;
    ColorMode logColor = ColorMode::Auto;
    bool generationMetrics = false;

    bool regional = false;
    std::int32_t regionThreads = 0;

    bool pluginsEnabled = true;
    std::int64_t pluginInstructionsPerTurn = 250000;
    std::int64_t pluginMemoryBytes = 32LL * 1024 * 1024;
    std::map<std::string, std::vector<std::string>, std::less<>> pluginAllowances;
    std::string orphanData = "warn";
    std::int32_t pluginLanes = 1;

    /// parity: `initial-enabled-packs` and `initial-disabled-packs`.
    std::vector<std::string> initialEnabledPacks{"vanilla"};
    std::vector<std::string> initialDisabledPacks;

    /// parity: `function-permission-level`, what a function's commands may do.
    std::int32_t functionPermissionLevel = 2;

    [[nodiscard]] static std::expected<BasaltConfig, std::string> fromToml(const wire::common::toml::TomlTable& root,
                                                                           std::string_view path);

    [[nodiscard]] static std::expected<BasaltConfig, std::string>
    load(const ConfigDirectory& directory = ConfigDirectory{},
         const std::string& legacyPropertiesPath = "server.properties",
         const wire::common::Logger& logger = wire::common::Logger{"Config"}, bool announce = true);

    [[nodiscard]] static std::optional<std::string>
    legacyMotd(const std::string& legacyPropertiesPath = "server.properties");
};

[[nodiscard]] std::map<std::string, std::string, std::less<>> parseJavaProperties(std::string_view text);

[[nodiscard]] std::string mergeLegacyProperties(std::string_view templateText,
                                                const std::map<std::string, std::string, std::less<>>& legacy);

} // namespace basalt::server::runtime::config
