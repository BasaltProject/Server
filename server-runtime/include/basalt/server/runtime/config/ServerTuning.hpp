#pragma once

#include <basalt/wire/common/toml/Toml.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::config {

struct ServerTuning {
    std::string brand;
    std::vector<std::string> startupCommands;
    bool forceGamemode = false;
    std::string usernamePattern;
    bool acceptsTransfers = false;
    bool allowFlight = false;
    bool preventProxyConnections = false;
    bool enforceWhitelist = false;
    std::int32_t playerAutosaveSeconds = 0;
    std::int32_t playerIdleTimeoutMinutes = 0;
    std::int32_t entityBroadcastRangePercentage = 100;
    std::int32_t maxChainedNeighborUpdates = 1000000;
    std::int32_t maxWorldSize = 29999984;

    std::int32_t compressionThreshold = 256;
    bool statusReplies = true;
    bool proxyProtocol = false;
    bool tcpNoDelay = true;
    bool queryEnabled = false;
    std::int32_t queryPort = 25565;
    std::int32_t connectionThrottleMs = 0;
    std::int32_t maxConnectionsPerAddress = 0;

    std::int32_t watchdogSeconds = 60;
    std::int32_t chunkSendRate = 0;

    bool logIps = true;

    [[nodiscard]] static std::expected<ServerTuning, std::string> fromToml(const wire::common::toml::TomlTable& root,
                                                                           std::string_view path);
};

} // namespace basalt::server::runtime::config
