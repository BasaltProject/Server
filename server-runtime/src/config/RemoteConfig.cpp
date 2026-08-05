#include <basalt/server/runtime/config/RemoteConfig.hpp>

#include <algorithm>
#include <initializer_list>

namespace basalt::server::runtime::config::remoteConfig {
namespace {

using wire::common::toml::TomlTable;

[[nodiscard]] std::expected<void, std::string> onlyKnown(const TomlTable& table, std::string_view name,
                                                         std::initializer_list<std::string_view> known,
                                                         std::string_view path) {
    for (const std::string& key : table.keys()) {
        if (std::ranges::find(known, key) == known.end()) {
            return std::unexpected(std::string{path} + ": unknown key \"" + key + "\" in [network." +
                                   std::string{name} + "]");
        }
    }
    return {};
}

[[nodiscard]] std::expected<std::int32_t, std::string> number(const TomlTable& table, std::string_view key,
                                                              std::int32_t fallback, std::int64_t low,
                                                              std::int64_t high, std::string_view section,
                                                              std::string_view path) {
    if (table.find(key) == nullptr) {
        return fallback;
    }
    const std::optional<std::int64_t> written = table.integer(key);
    if (!written.has_value() || *written < low || *written > high) {
        return std::unexpected(std::string{path} + ": " + std::string{key} + " in [network." + std::string{section} +
                               "] must be a number from " + std::to_string(low) + " to " + std::to_string(high));
    }
    return static_cast<std::int32_t>(*written);
}

#define BASALT_TRY_NUMBER(target, table, key, low, high, section)                                                      \
    {                                                                                                                  \
        auto read = number(table, key, target, low, high, section, path);                                              \
        if (!read.has_value()) {                                                                                       \
            return std::unexpected(read.error());                                                                      \
        }                                                                                                              \
        target = *read;                                                                                                \
    }

} // namespace

std::expected<remote::RemoteSettings, std::string> fromToml(const TomlTable& root, std::string_view path) {
    remote::RemoteSettings settings;
    const TomlTable* network = root.table("network");
    if (const TomlTable* rcon = network == nullptr ? nullptr : network->table("rcon"); rcon != nullptr) {
        if (auto known = onlyKnown(*rcon, "rcon",
                                   {"enabled", "bind", "port", "password", "max-failures", "lockout-seconds"}, path);
            !known.has_value()) {
            return std::unexpected(known.error());
        }
        remote::RconSettings& out = settings.rcon;
        out.enabled = rcon->boolean("enabled").value_or(out.enabled);
        out.bind = std::string{rcon->string("bind").value_or(out.bind)};
        out.password = std::string{rcon->string("password").value_or("")};
        BASALT_TRY_NUMBER(out.port, *rcon, "port", 1, 65535, "rcon")
        BASALT_TRY_NUMBER(out.maxFailures, *rcon, "max-failures", 1, 1000, "rcon")
        BASALT_TRY_NUMBER(out.lockoutSeconds, *rcon, "lockout-seconds", 1, 86400, "rcon")
    }
    if (const TomlTable* api = network == nullptr ? nullptr : network->table("api"); api != nullptr) {
        if (auto known =
                onlyKnown(*api, "api",
                          {"enabled", "bind", "port", "tls", "tls-cert", "tls-key", "max-failures", "lockout-seconds",
                           "requests-per-minute", "max-connections", "max-body-bytes", "idle-seconds"},
                          path);
            !known.has_value()) {
            return std::unexpected(known.error());
        }
        remote::ApiSettings& out = settings.api;
        out.enabled = api->boolean("enabled").value_or(out.enabled);
        out.bind = std::string{api->string("bind").value_or(out.bind)};
        out.tls = api->boolean("tls").value_or(out.tls);
        out.certPath = std::string{api->string("tls-cert").value_or("")};
        out.keyPath = std::string{api->string("tls-key").value_or("")};
        BASALT_TRY_NUMBER(out.port, *api, "port", 1, 65535, "api")
        BASALT_TRY_NUMBER(out.maxFailures, *api, "max-failures", 1, 1000, "api")
        BASALT_TRY_NUMBER(out.lockoutSeconds, *api, "lockout-seconds", 1, 86400, "api")
        BASALT_TRY_NUMBER(out.requestsPerMinute, *api, "requests-per-minute", 1, 1000000, "api")
        BASALT_TRY_NUMBER(out.maxConnections, *api, "max-connections", 1, 4096, "api")
        BASALT_TRY_NUMBER(out.maxBodyBytes, *api, "max-body-bytes", 1, 16777216, "api")
        BASALT_TRY_NUMBER(out.idleSeconds, *api, "idle-seconds", 1, 86400, "api")
        if (out.enabled && out.tls && (out.certPath.empty() || out.keyPath.empty())) {
            return std::unexpected(std::string{path} + ": tls = true in [network.api] needs tls-cert and tls-key");
        }
    }
    if (settings.rcon.enabled && settings.rcon.port == settings.api.port && settings.api.enabled &&
        settings.rcon.bind == settings.api.bind) {
        return std::unexpected(std::string{path} + ": [network.rcon] and [network.api] cannot share one port");
    }
    return settings;
}

} // namespace basalt::server::runtime::config::remoteConfig
