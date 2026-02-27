#include <basalt/server/runtime/config/BasaltConfig.hpp>
#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/server/runtime/config/ForwardingConfig.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::config {
namespace {

using wire::common::LogLevel;
using wire::common::placeholder::lowercaseAscii;
using wire::common::toml::TomlTable;

/// A whole number as written, or nothing when it is anything else. `from_chars` stops at the first
/// character it cannot use and calls that a success, so the end has to be the end.
[[nodiscard]] std::optional<std::int64_t> asInteger(std::string_view value) {
    std::int64_t parsed = 0;
    const std::from_chars_result read = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (read.ec != std::errc{} || read.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return parsed;
}

/// parity of shape with Folia's own `threads = -1`: a number is taken as it is, and anything else
/// ("auto", missing) means "work it out", which is zero here and is resolved by the scheduler
/// rather than by the file.
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

/// What to write into a freshly created `server.toml`: an imported `server.properties`, or the
/// default.
[[nodiscard]] std::string initialContents(const std::string& legacyPropertiesPath, const wire::common::Logger& logger) {
    const std::optional<std::string> legacy = wire::io::fileIO::readText(legacyPropertiesPath);
    // A server with nothing beside it is the common case, and reading the absent file below would
    // be undefined behaviour rather than a wrong answer.
    if (!legacy.has_value()) {
        return std::string{kServerDefaultToml};
    }
    logger.info(
        [&legacyPropertiesPath] { return "Found " + legacyPropertiesPath + ", importing recognized values once."; });
    return mergeLegacyProperties(kServerDefaultToml, parseJavaProperties(*legacy));
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
        // Strictly true or false, as Java reads it: anything else is not a decision to import.
        if (*online == "true" || *online == "false") {
            replaceOnce("online-mode = true", "online-mode = " + std::string{*online});
        }
    }
    // The view distance is not imported here because it no longer lives in this file: it is a
    // per-world setting and belongs to `worlds.toml`, which is where importing it goes when it
    // does.
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
        // The port sat here until the two editions each got their own door. Said out loud rather
        // than ignored: a server that quietly went back to 25565 is a server nobody can reach.
        if (server->find("port") != nullptr) {
            return std::unexpected(std::string{path} + ": `port` moved to [network.java], beside the Bedrock one");
        }
        config.maxPlayers = static_cast<std::int32_t>(server->integer("max-players").value_or(defaults.maxPlayers));
        config.onlineMode = server->boolean("online-mode").value_or(defaults.onlineMode);
        config.gamemode = std::string{server->string("gamemode").value_or(defaults.gamemode)};
        config.whitelist = server->boolean("whitelist").value_or(defaults.whitelist);
    }

    // The two doors, side by side: neither edition is a layer on top of the other.
    if (const TomlTable* network = root.table("network"); network != nullptr) {
        if (const TomlTable* java = network->table("java"); java != nullptr) {
            config.javaEnabled = java->boolean("enabled").value_or(defaults.javaEnabled);
            const std::optional<std::int64_t> port = java->integer("port");
            if (port.has_value() && (*port < 1 || *port > 65535)) {
                return std::unexpected(std::string{path} + ": the java port must be between 1 and 65535");
            }
            config.port = static_cast<std::int32_t>(port.value_or(defaults.port));
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

    if (plugins != nullptr) {
        config.pluginsEnabled = plugins->boolean("enabled").value_or(defaults.pluginsEnabled);
        // Floored rather than taken as written: a budget of five instructions or a megabyte of
        // nothing stops every plugin on the server, and reads as "plugins are broken" rather than
        // as the typo it is.
        config.pluginInstructionsPerTurn = std::max<std::int64_t>(
            plugins->integer("instructions-per-turn").value_or(defaults.pluginInstructionsPerTurn), 1000);
        config.pluginMemoryBytes =
            std::max<std::int64_t>(plugins->integer("memory-mb").value_or(defaults.pluginMemoryBytes / 1024 / 1024),
                                   1) *
            1024 * 1024;
        config.pluginLanes =
            static_cast<std::int32_t>(std::max<std::int64_t>(plugins->integer("test-lanes").value_or(1), 1));
        config.orphanData = std::string{plugins->string("orphan-data").value_or(defaults.orphanData)};
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
