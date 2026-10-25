#include <basalt/server/runtime/config/BasaltConfig.hpp>
#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/server/runtime/config/ForwardingConfig.hpp>
#include <basalt/server/runtime/config/RemoteConfig.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/io/TcpSocket.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::config {
namespace {

using wire::common::LogLevel;
using wire::common::placeholder::lowercaseAscii;
using wire::common::toml::TomlTable;

// `from_chars` calls a stop at the first unusable character a success, so the end has to be the end.
[[nodiscard]] std::optional<std::int64_t> asInteger(std::string_view value) {
    std::int64_t parsed = 0;
    const std::from_chars_result read = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (read.ec != std::errc{} || read.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return parsed;
}

// parity of shape with Folia's `threads = -1`: anything but a number means auto, resolved by the scheduler.
[[nodiscard]] std::int32_t regionThreadsOf(std::optional<std::string_view> value) {
    if (!value.has_value()) {
        return 0;
    }
    const std::optional<std::int64_t> parsed = asInteger(*value);
    return parsed.has_value() ? static_cast<std::int32_t>(*parsed) : 0;
}

[[nodiscard]] ColorMode parseColorMode(std::string_view name, ColorMode fallback) {
    for (const ColorMode mode : {ColorMode::Auto, ColorMode::Always, ColorMode::Never}) {
        if (lowercaseAscii(name) == colorModeName(mode)) {
            return mode;
        }
    }
    return fallback;
}

[[nodiscard]] LogLevel parseLogLevel(std::string_view name, LogLevel fallback) {
    for (const LogLevel level : {LogLevel::Trace, LogLevel::Debug, LogLevel::Info, LogLevel::Warn, LogLevel::Error}) {
        if (lowercaseAscii(name) == lowercaseAscii(logLevelLabel(level))) {
            return level;
        }
    }
    return fallback;
}

[[nodiscard]] std::string initialContents(const std::string& legacyPropertiesPath, const wire::common::Logger& logger) {
    const std::optional<std::string> legacy = wire::io::fileIO::readText(legacyPropertiesPath);
    // Reading the absent file below would be undefined behaviour rather than a wrong answer.
    if (!legacy.has_value()) {
        return std::string{kServerDefaultToml};
    }
    logger.info(
        [&legacyPropertiesPath] { return "Found " + legacyPropertiesPath + ", importing recognized values once."; });
    return mergeLegacyProperties(kServerDefaultToml, parseJavaProperties(*legacy));
}

void replaceAfter(std::string& text, std::string_view marker, std::string_view written, const std::string& with) {
    const std::size_t from = text.find(marker);
    const std::size_t at = from == std::string::npos ? std::string::npos : text.find(written, from);
    if (at != std::string::npos) {
        text.replace(at, written.size(), with);
    }
}

// The vanilla properties this build reads from server.toml, by the line the template carries them on.
void importTuning(std::string& result, const std::map<std::string, std::string, std::less<>>& legacy) {
    struct Row {
        std::string_view property;
        std::string_view marker;
        std::string_view line;
        bool boolean;
    };

    static constexpr std::array<Row, 14> kRows{{
        {"allow-flight", "[server]", "allow-flight = false", true},
        {"force-gamemode", "[server]", "force-gamemode = false", true},
        {"enforce-whitelist", "[server]", "enforce-whitelist = false", true},
        {"accepts-transfers", "[server]", "accepts-transfers = false", true},
        {"prevent-proxy-connections", "[server]", "prevent-proxy-connections = false", true},
        {"player-idle-timeout", "[server]", "player-idle-timeout = 0", false},
        {"entity-broadcast-range-percentage", "[server]", "entity-broadcast-range-percentage = 100", false},
        {"max-chained-neighbor-updates", "[server]", "max-chained-neighbor-updates = 1000000", false},
        {"max-world-size", "[server]", "max-world-size = 29999984", false},
        {"network-compression-threshold", "[network.java]", "compression-threshold = 256", false},
        {"enable-status", "[network.java]", "status-replies = true", true},
        {"enable-query", "[network.query]", "enabled = false", true},
        {"query.port", "[network.query]", "port = 25565", false},
        {"log-ips", "[logging]", "log-ips = true", true},
    }};
    for (const Row& row : kRows) {
        const auto found = legacy.find(row.property);
        if (found == legacy.end()) {
            continue;
        }
        const std::string_view value = found->second;
        const std::string key{row.line.substr(0, row.line.find(' '))};
        if (row.boolean ? (value == "true" || value == "false") : asInteger(value).has_value()) {
            replaceAfter(result, row.marker, row.line, key + " = " + std::string{value});
        }
    }
    // parity: `max-tick-time` is milliseconds and `watchdog-seconds` is seconds; -1 turned the watchdog off.
    if (const auto found = legacy.find("max-tick-time"); found != legacy.end()) {
        if (const std::optional<std::int64_t> millis = asInteger(found->second); millis.has_value()) {
            replaceAfter(result, "[performance]", "watchdog-seconds = 60",
                         "watchdog-seconds = " + std::to_string(*millis <= 0 ? 0 : (*millis + 999) / 1000));
        }
    }
}

} // namespace

std::string_view colorModeName(ColorMode mode) noexcept {
    switch (mode) {
    case ColorMode::Always:
        return "always";
    case ColorMode::Never:
        return "never";
    case ColorMode::Auto:
    default:
        return "auto";
    }
}

std::map<std::string, std::string, std::less<>> parseJavaProperties(std::string_view text) {
    std::map<std::string, std::string, std::less<>> values;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        std::string_view line =
            text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        start = end == std::string_view::npos ? text.size() + 1 : end + 1;
        while (!line.empty() && static_cast<unsigned char>(line.front()) <= ' ') {
            line.remove_prefix(1);
        }
        while (!line.empty() && static_cast<unsigned char>(line.back()) <= ' ') {
            line.remove_suffix(1);
        }
        if (line.empty() || line.front() == '#' || line.front() == '!') {
            continue;
        }
        const std::size_t separator = line.find_first_of("=:");
        if (separator == std::string_view::npos) {
            continue;
        }
        std::string_view key = line.substr(0, separator);
        std::string_view value = line.substr(separator + 1);
        while (!key.empty() && static_cast<unsigned char>(key.back()) <= ' ') {
            key.remove_suffix(1);
        }
        while (!value.empty() && static_cast<unsigned char>(value.front()) <= ' ') {
            value.remove_prefix(1);
        }
        values.emplace(std::string{key}, std::string{value});
    }
    return values;
}

std::string mergeLegacyProperties(std::string_view templateText,
                                  const std::map<std::string, std::string, std::less<>>& legacy) {
    std::string result{templateText};
    const auto replaceOnce = [&result](std::string_view written, const std::string& with) {
        const std::size_t at = result.find(written);
        if (at != std::string::npos) {
            result.replace(at, written.size(), with);
        }
    };
    const auto imported = [&legacy](std::string_view key) -> std::optional<std::string_view> {
        const auto at = legacy.find(key);
        return at == legacy.end() ? std::nullopt : std::optional<std::string_view>{at->second};
    };

    if (const auto port = imported("server-port"); port.has_value()) {
        if (const std::optional<std::int64_t> number = asInteger(*port); number.has_value()) {
            replaceOnce("port = 25565", "port = " + std::to_string(*number));
        }
    }
    if (const auto players = imported("max-players"); players.has_value()) {
        if (const std::optional<std::int64_t> number = asInteger(*players); number.has_value()) {
            replaceOnce("max-players = 80", "max-players = " + std::to_string(*number));
        }
    }
    if (const auto online = imported("online-mode"); online.has_value()) {
        if (*online == "true" || *online == "false") {
            replaceOnce("online-mode = true", "online-mode = " + std::string{*online});
        }
    }
    for (const auto& [key, fallback] :
         {std::pair{"pause-when-empty-seconds", "60"}, std::pair{"spawn-protection", "16"}}) {
        if (const auto value = imported(key); value.has_value() && asInteger(*value).has_value()) {
            replaceOnce(std::string{key} + " = " + fallback,
                        std::string{key} + " = " + std::to_string(*asInteger(*value)));
        }
    }
    if (const auto enforce = imported("enforce-secure-profile"); enforce.has_value()) {
        if (*enforce == "true" || *enforce == "false") {
            replaceOnce("enforce-secure-profile = true", "enforce-secure-profile = " + std::string{*enforce});
        }
    }
    if (const auto link = imported("bug-report-link");
        link.has_value() && link->find_first_of("\"\\\n") == std::string_view::npos) {
        replaceOnce("bug-report-link = \"\"", "bug-report-link = \"" + std::string{*link} + "\"");
    }
    const auto tomlString = [](std::string_view text) {
        std::string quoted = "\"";
        for (const char c : text) {
            if (c == '"' || c == '\\') {
                quoted += '\\';
            }
            quoted += c;
        }
        return quoted + "\"";
    };
    for (const std::string_view key :
         {"resource-pack", "resource-pack-sha1", "resource-pack-id", "resource-pack-prompt"}) {
        if (const auto value = imported(key);
            value.has_value() && value->find_first_of("\n\r") == std::string_view::npos) {
            replaceOnce(std::string{key} + " = \"\"", std::string{key} + " = " + tomlString(*value));
        }
    }
    if (const auto required = imported("require-resource-pack"); required == "true" || required == "false") {
        replaceOnce("require-resource-pack = false", "require-resource-pack = " + std::string{*required});
    }
    // parity: `DedicatedServerProperties.getDatapackConfig`, comma-separated and trimmed.
    const auto packList = [](std::string_view listed) {
        std::string toml = "[";
        std::size_t start = 0;
        while (start <= listed.size()) {
            const std::size_t comma = std::min(listed.find(',', start), listed.size());
            std::string_view one = listed.substr(start, comma - start);
            while (!one.empty() && one.front() == ' ') {
                one.remove_prefix(1);
            }
            while (!one.empty() && one.back() == ' ') {
                one.remove_suffix(1);
            }
            if (!one.empty()) {
                toml += (toml.size() > 1 ? ", \"" : "\"") + std::string{one} + "\"";
            }
            start = comma + 1;
        }
        return toml + "]";
    };
    if (const auto enabled = imported("initial-enabled-packs"); enabled.has_value()) {
        replaceOnce("initial-enabled = [\"vanilla\"]", "initial-enabled = " + packList(*enabled));
    }
    if (const auto disabled = imported("initial-disabled-packs"); disabled.has_value()) {
        replaceOnce("initial-disabled = []", "initial-disabled = " + packList(*disabled));
    }
    if (const auto level = imported("function-permission-level"); level.has_value()) {
        if (const std::optional<std::int64_t> number = asInteger(*level); number.has_value()) {
            replaceOnce("function-permission-level = 2", "function-permission-level = " + std::to_string(*number));
        }
    }
    importTuning(result, legacy);
    // The view distance is a per-world setting that belongs to `worlds.toml`, so it is not imported here.
    return result;
}

std::optional<std::string> BasaltConfig::legacyMotd(const std::string& legacyPropertiesPath) {
    const std::optional<std::string> text = wire::io::fileIO::readText(legacyPropertiesPath);
    if (!text.has_value()) {
        return std::nullopt;
    }
    const std::map<std::string, std::string, std::less<>> values = parseJavaProperties(*text);
    const auto motd = values.find("motd");
    return motd == values.end() ? std::nullopt : std::optional<std::string>{motd->second};
}

std::expected<BasaltConfig, std::string> BasaltConfig::fromToml(const TomlTable& root, std::string_view path) {
    const BasaltConfig defaults;
    BasaltConfig config;
    const TomlTable* server = root.table("server");
    const TomlTable* logging = root.table("logging");
    const TomlTable* performance = root.table("performance");
    const TomlTable* plugins = root.table("plugins");

    if (server != nullptr) {
        config.maxPlayers = static_cast<std::int32_t>(server->integer("max-players").value_or(defaults.maxPlayers));
        config.onlineMode = server->boolean("online-mode").value_or(defaults.onlineMode);
        config.enforceSecureProfile = server->boolean("enforce-secure-profile").value_or(defaults.enforceSecureProfile);
        config.bugReportLink = std::string{server->string("bug-report-link").value_or(defaults.bugReportLink)};
        config.resourcePackUrl = std::string{server->string("resource-pack").value_or(defaults.resourcePackUrl)};
        config.resourcePackSha1 = std::string{server->string("resource-pack-sha1").value_or(defaults.resourcePackSha1)};
        config.resourcePackId = std::string{server->string("resource-pack-id").value_or(defaults.resourcePackId)};
        config.requireResourcePack = server->boolean("require-resource-pack").value_or(defaults.requireResourcePack);
        config.resourcePackPrompt =
            std::string{server->string("resource-pack-prompt").value_or(defaults.resourcePackPrompt)};
        config.gamemode = std::string{server->string("gamemode").value_or(defaults.gamemode)};
        config.whitelist = server->boolean("whitelist").value_or(defaults.whitelist);
        config.pauseWhenEmptySeconds = static_cast<std::int32_t>(std::max<std::int64_t>(
            server->integer("pause-when-empty-seconds").value_or(defaults.pauseWhenEmptySeconds), 0));
        config.spawnProtection = static_cast<std::int32_t>(
            std::max<std::int64_t>(server->integer("spawn-protection").value_or(defaults.spawnProtection), 0));
    }

    if (const TomlTable* network = root.table("network"); network != nullptr) {
        if (const TomlTable* java = network->table("java"); java != nullptr) {
            config.javaEnabled = java->boolean("enabled").value_or(defaults.javaEnabled);
            const std::optional<std::int64_t> port = java->integer("port");
            if (port.has_value() && (*port < 1 || *port > 65535)) {
                return std::unexpected(std::string{path} + ": the java port must be between 1 and 65535");
            }
            config.port = static_cast<std::int32_t>(port.value_or(defaults.port));
            if ((java->find("bind") && !java->string("bind")) ||
                (java->find("dual-stack") && !java->boolean("dual-stack")))
                return std::unexpected(std::string{path} + ": java bind must be a string and dual-stack a boolean");
            config.javaBind = std::string{java->string("bind").value_or(defaults.javaBind)};
            config.javaDualStack = java->boolean("dual-stack").value_or(defaults.javaDualStack);
            if (!wire::io::TcpAddress::literal(config.javaBind, static_cast<std::uint16_t>(config.port)))
                return std::unexpected(std::string{path} + ": java bind must be a literal IPv4 or IPv6 address");
            if (config.javaDualStack && config.javaBind.find(':') == std::string::npos)
                return std::unexpected(std::string{path} + ": java dual-stack requires an IPv6 bind address");
        }
        if (const TomlTable* bedrock = network->table("bedrock"); bedrock != nullptr) {
            config.bedrockEnabled = bedrock->boolean("enabled").value_or(defaults.bedrockEnabled);
            const std::optional<std::int64_t> port = bedrock->integer("port");
            if (port.has_value() && (*port < 1 || *port > 65535)) {
                return std::unexpected(std::string{path} + ": the bedrock port must be between 1 and 65535");
            }
            config.bedrockPort = static_cast<std::int32_t>(port.value_or(defaults.bedrockPort));
            config.bedrockPrefix = std::string{bedrock->string("prefix").value_or(defaults.bedrockPrefix)};
        }
    }
    if (!config.javaEnabled && !config.bedrockEnabled) {
        return std::unexpected(std::string{path} + ": both editions are off, so nobody can join at all");
    }

    std::expected<wire::auth::forwarding::ProxyForwarding, std::string> forwarding =
        forwardingConfig::fromToml(root, path);
    if (!forwarding.has_value()) {
        return std::unexpected(forwarding.error());
    }
    config.forwarding = std::move(*forwarding);

    std::expected<remote::RemoteSettings, std::string> remoteSettings = remoteConfig::fromToml(root, path);
    if (!remoteSettings.has_value()) {
        return std::unexpected(remoteSettings.error());
    }
    config.remote = std::move(*remoteSettings);
    std::expected<ServerTuning, std::string> tuning = ServerTuning::fromToml(root, path);
    if (!tuning.has_value()) {
        return std::unexpected(tuning.error());
    }
    config.tuning = std::move(*tuning);

    if (logging != nullptr) {
        const std::optional<std::string_view> level = logging->string("level");
        config.logLevel = level.has_value() ? parseLogLevel(*level, defaults.logLevel) : defaults.logLevel;
        config.logToFile = logging->boolean("file").value_or(defaults.logToFile);
        config.logKeepDays = static_cast<std::int32_t>(logging->integer("keep-days").value_or(defaults.logKeepDays));
        const std::optional<std::string_view> color = logging->string("color");
        config.logColor = color.has_value() ? parseColorMode(*color, defaults.logColor) : defaults.logColor;
    }

    if (performance != nullptr) {
        config.generationMetrics = performance->boolean("generation-metrics").value_or(defaults.generationMetrics);
        config.regional = performance->boolean("regional").value_or(defaults.regional);
        config.regionThreads = regionThreadsOf(performance->string("region-threads"));
    }

    if (const TomlTable* datapacks = root.table("datapacks"); datapacks != nullptr) {
        if (datapacks->find("initial-enabled") != nullptr) {
            config.initialEnabledPacks = datapacks->stringList("initial-enabled");
        }
        config.initialDisabledPacks = datapacks->stringList("initial-disabled");
        const std::int64_t level =
            datapacks->integer("function-permission-level").value_or(defaults.functionPermissionLevel);
        if (level < 1 || level > 4) {
            return std::unexpected(std::string{path} + ": function-permission-level must be between 1 and 4");
        }
        config.functionPermissionLevel = static_cast<std::int32_t>(level);
    }

    if (plugins != nullptr) {
        config.pluginsEnabled = plugins->boolean("enabled").value_or(defaults.pluginsEnabled);
        // Floored: a budget of five instructions stops every plugin and reads as plugins broken, not as a typo.
        config.pluginInstructionsPerTurn = std::max<std::int64_t>(
            plugins->integer("instructions-per-turn").value_or(defaults.pluginInstructionsPerTurn), 1000);
        config.pluginMemoryBytes =
            std::max<std::int64_t>(plugins->integer("memory-mb").value_or(defaults.pluginMemoryBytes / 1024 / 1024),
                                   1) *
            1024 * 1024;
        config.pluginLanes =
            static_cast<std::int32_t>(std::max<std::int64_t>(plugins->integer("test-lanes").value_or(1), 1));
        config.orphanData = std::string{plugins->string("orphan-data").value_or(defaults.orphanData)};
        config.pluginTrustedKeys = plugins->stringList("trusted-keys");
        if (const TomlTable* allow = plugins->table("allow"); allow != nullptr) {
            for (const std::string& id : allow->keys()) {
                config.pluginAllowances.emplace(id, allow->stringList(id));
            }
        }
    }
    return config;
}

std::expected<BasaltConfig, std::string> BasaltConfig::load(const ConfigDirectory& directory,
                                                            const std::string& legacyPropertiesPath,
                                                            const wire::common::Logger& logger, bool announce) {
    const std::string path = directory.pathOf(kFileName);
    if (!wire::io::fileIO::exists(path)) {
        wire::io::fileIO::createDirectories(directory.root());
        wire::io::fileIO::writeText(path, initialContents(legacyPropertiesPath, logger));
    }
    const std::expected<TomlTable, std::string> root = directory.load(kFileName, kServerDefaultToml);
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }
    if (announce) {
        logger.info([&path] { return "Loaded configuration from " + path + "."; });
    }
    return fromToml(*root, path);
}

} // namespace basalt::server::runtime::config
