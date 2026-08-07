#include <basalt/server/runtime/config/ServerTuning.hpp>

#include <regex>

namespace basalt::server::runtime::config {
namespace {

using wire::common::toml::TomlTable;

[[nodiscard]] std::expected<std::int32_t, std::string> ranged(const TomlTable& table, std::string_view key,
                                                              std::int32_t fallback, std::int64_t low,
                                                              std::int64_t high, std::string_view path) {
    if (table.find(key) != nullptr && !table.integer(key).has_value()) {
        return std::unexpected(std::string{path} + ": " + std::string{key} + " must be a whole number");
    }
    const std::int64_t value = table.integer(key).value_or(fallback);
    if (value < low || value > high) {
        return std::unexpected(std::string{path} + ": " + std::string{key} + " must be between " + std::to_string(low) +
                               " and " + std::to_string(high));
    }
    return static_cast<std::int32_t>(value);
}

} // namespace

std::expected<ServerTuning, std::string> ServerTuning::fromToml(const TomlTable& root, std::string_view path) {
    ServerTuning tuning;
    if (const TomlTable* server = root.table("server"); server != nullptr) {
        tuning.brand = std::string{server->string("brand").value_or(tuning.brand)};
        tuning.startupCommands = server->stringList("startup-commands");
        tuning.usernamePattern = std::string{server->string("username-pattern").value_or(tuning.usernamePattern)};
        if (!tuning.usernamePattern.empty()) {
            try {
                static_cast<void>(std::regex{tuning.usernamePattern});
            } catch (const std::regex_error& error) {
                return std::unexpected(std::string{path} +
                                       ": username-pattern is not a valid regular expression: " + error.what());
            }
        }
        tuning.forceGamemode = server->boolean("force-gamemode").value_or(tuning.forceGamemode);
        tuning.acceptsTransfers = server->boolean("accepts-transfers").value_or(tuning.acceptsTransfers);
        tuning.allowFlight = server->boolean("allow-flight").value_or(tuning.allowFlight);
        tuning.preventProxyConnections =
            server->boolean("prevent-proxy-connections").value_or(tuning.preventProxyConnections);
        tuning.enforceWhitelist = server->boolean("enforce-whitelist").value_or(tuning.enforceWhitelist);
        const auto idle = ranged(*server, "player-idle-timeout", tuning.playerIdleTimeoutMinutes, 0, 1 << 24, path);
        if (!idle.has_value()) {
            return std::unexpected(idle.error());
        }
        tuning.playerIdleTimeoutMinutes = *idle;
        const auto broadcast =
            ranged(*server, "entity-broadcast-range-percentage", tuning.entityBroadcastRangePercentage, 10, 1000, path);
        if (!broadcast.has_value()) {
            return std::unexpected(broadcast.error());
        }
        tuning.entityBroadcastRangePercentage = *broadcast;
        const auto autosave =
            ranged(*server, "player-autosave-seconds", tuning.playerAutosaveSeconds, 0, 1 << 24, path);
        if (!autosave.has_value()) {
            return std::unexpected(autosave.error());
        }
        tuning.playerAutosaveSeconds = *autosave;
        const auto chained =
            ranged(*server, "max-chained-neighbor-updates", tuning.maxChainedNeighborUpdates, -1, 1 << 30, path);
        if (!chained.has_value()) {
            return std::unexpected(chained.error());
        }
        tuning.maxChainedNeighborUpdates = *chained;
        const auto worldSize = ranged(*server, "max-world-size", tuning.maxWorldSize, 1, 29999984, path);
        if (!worldSize.has_value()) {
            return std::unexpected(worldSize.error());
        }
        tuning.maxWorldSize = *worldSize;
    }
    if (const TomlTable* network = root.table("network"); network != nullptr) {
        if (const TomlTable* java = network->table("java"); java != nullptr) {
            const auto threshold =
                ranged(*java, "compression-threshold", tuning.compressionThreshold, -1, 1 << 24, path);
            if (!threshold.has_value()) {
                return std::unexpected(threshold.error());
            }
            tuning.compressionThreshold = *threshold;
            tuning.statusReplies = java->boolean("status-replies").value_or(tuning.statusReplies);
            tuning.proxyProtocol = java->boolean("proxy-protocol").value_or(tuning.proxyProtocol);
            tuning.tcpNoDelay = java->boolean("tcp-no-delay").value_or(tuning.tcpNoDelay);
            const auto throttle =
                ranged(*java, "connection-throttle-ms", tuning.connectionThrottleMs, 0, 1 << 24, path);
            const auto perAddress =
                ranged(*java, "max-connections-per-address", tuning.maxConnectionsPerAddress, 0, 1 << 20, path);
            if (!throttle.has_value() || !perAddress.has_value()) {
                return std::unexpected(!throttle.has_value() ? throttle.error() : perAddress.error());
            }
            tuning.connectionThrottleMs = *throttle;
            tuning.maxConnectionsPerAddress = *perAddress;
        }
    }
    if (const TomlTable* network = root.table("network"); network != nullptr) {
        if (const TomlTable* query = network->table("query"); query != nullptr) {
            tuning.queryEnabled = query->boolean("enabled").value_or(tuning.queryEnabled);
            const auto port = ranged(*query, "port", tuning.queryPort, 1, 65535, path);
            if (!port.has_value()) {
                return std::unexpected(port.error());
            }
            tuning.queryPort = *port;
        }
    }
    if (const TomlTable* performance = root.table("performance"); performance != nullptr) {
        const auto watchdog = ranged(*performance, "watchdog-seconds", tuning.watchdogSeconds, 0, 1 << 20, path);
        if (!watchdog.has_value()) {
            return std::unexpected(watchdog.error());
        }
        tuning.watchdogSeconds = *watchdog;
        const auto chunkRate = ranged(*performance, "chunk-send-rate", tuning.chunkSendRate, 0, 1280, path);
        if (!chunkRate.has_value()) {
            return std::unexpected(chunkRate.error());
        }
        tuning.chunkSendRate = *chunkRate;
    }
    if (const TomlTable* logging = root.table("logging"); logging != nullptr) {
        tuning.logIps = logging->boolean("log-ips").value_or(tuning.logIps);
    }
    return tuning;
}

} // namespace basalt::server::runtime::config
