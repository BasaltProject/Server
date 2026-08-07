#pragma once

#include <basalt/server/plugins/PluginPackets.hpp>
#include <basalt/server/runtime/config/ConfigDirectory.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::config {

struct PacketRule {
    enum class Action : std::uint8_t { Drop, Kick };

    std::string name;
    server::plugins::PacketAddress address;
    std::int32_t maxPerSecond = 0;
    Action action = Action::Drop;
};

/// Longest text, in characters, a client may put on an item it hands the server whole; 0 means no limit.
struct ItemCaps {
    std::int32_t customNameLength = 0;
    std::int32_t loreLineLength = 0;
    std::int32_t bookPageLength = 0;
    std::int32_t bookTitleLength = 0;
};

struct LimitsConfig {
    static constexpr std::string_view kFileName = "limits.toml";

    std::int32_t packetRateLimit = 0;
    std::vector<PacketRule> packetRules;
    std::int32_t chatSpamThresholdSeconds = 10;
    std::int32_t commandSpamThresholdSeconds = 10;
    std::vector<std::string> spamExcludedCommands;
    ItemCaps items;

    [[nodiscard]] static std::expected<LimitsConfig, std::string>
    load(const ConfigDirectory& directory = ConfigDirectory{},
         const wire::common::Logger& logger = wire::common::Logger{"Config"});
};

} // namespace basalt::server::runtime::config
