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

/// When the console gets ANSI colour. `Auto` means "only when stdout is a terminal".
enum class ColorMode : std::uint8_t { Auto, Always, Never };

[[nodiscard]] std::string_view colorModeName(ColorMode mode) noexcept;

/// Parsed `config/server.toml`. Only the fields Basalt currently reads are exposed; everything else
/// is a documented placeholder in the file itself.
///
/// What is left here is what has no larger domain of its own. Worlds left for `worlds.toml`, the
/// message of the day for `server_list.toml`, and operators for `groups.toml` plus a column in
/// `players.db`.
struct BasaltConfig {
    /// The file this configuration lives in, inside a `ConfigDirectory`.
    static constexpr std::string_view kFileName = "server.toml";

    /// The two editions are two doors into the same server, and either can be shut: a server for
    /// phones and consoles only is a real thing to want, and so is one nobody on a phone reaches.
    /// Shutting both is refused, because then nobody can join at all.
    bool javaEnabled = true;
    std::int32_t port = 25565;

    bool bedrockEnabled = true;
    std::int32_t bedrockPort = 19132;
    /// What goes in front of a Bedrock player's name inside the server, so a Steve from each
    /// edition is two people. Empty is a real choice on a server where only Bedrock players join.
    std::string bedrockPrefix = ".";
    std::int32_t maxPlayers = 80;
    bool onlineMode = true;
    std::string gamemode = "survival";
    /// Whether only whitelisted players may join. Who is whitelisted lives in `players.db`.
    bool whitelist = false;
    /// Which proxy, if any, is speaking for the players that reach this server. See
    /// `[network.forwarding]`.
    wire::auth::forwarding::ProxyForwarding forwarding;
    wire::common::LogLevel logLevel = wire::common::LogLevel::Info;
    bool logToFile = true;
    std::int32_t logKeepDays = 14;
    ColorMode logColor = ColorMode::Auto;
    bool generationMetrics = false;

    /// Whether a world may be split into regions that tick independently.
    ///
    /// NOT VANILLA above one region: two of them stop having a fixed order of tick between them
    /// and what crosses a border is deferred a tick, so it is off unless the operator asks for it.
    bool regional = false;
    /// How many regions tick at once with `regional` on. Zero means "one per pair of cores".
    std::int32_t regionThreads = 0;

    /// Whether the plugins folder is looked at. Off means it is not even read.
    bool pluginsEnabled = true;
    /// What one plugin may run in a single turn, refilled on every entry. See `[plugins]`.
    std::int64_t pluginInstructionsPerTurn = 250000;
    /// The most memory one plugin's own objects may take.
    std::int64_t pluginMemoryBytes = 32LL * 1024 * 1024;
    /// What each plugin is allowed beyond its own sandbox, by id.
    ///
    /// Empty for almost every server: only `packets`, `network` and `commands` are asked for here,
    /// and a plugin that wants one and is not listed loads without it and says so.
    std::map<std::string, std::vector<std::string>, std::less<>> pluginAllowances;
    /// Whether orphaned plugin data is reported at startup: `warn` or `quiet`.
    std::string orphanData = "warn";
    /// How many copies of each plugin run. One on a server; more is a development switch.
    std::int32_t pluginLanes = 1;

    /// Reads what the file says, leaving every option it does not mention at its default.
    [[nodiscard]] static std::expected<BasaltConfig, std::string> fromToml(const wire::common::toml::TomlTable& root,
                                                                           std::string_view path);

    /// Loads `config/server.toml`, generating a commented default on first run. A vanilla
    /// `server.properties` beside it has its overlapping keys imported once, so moving an existing
    /// server over is "copy the folder and start". An import, not a compatibility layer.
    ///
    /// `announce` is false for the boot read that only exists to set up logging.
    [[nodiscard]] static std::expected<BasaltConfig, std::string>
    load(const ConfigDirectory& directory = ConfigDirectory{},
         const std::string& legacyPropertiesPath = "server.properties",
         const wire::common::Logger& logger = wire::common::Logger{"Config"}, bool announce = true);

    /// The `motd` of a vanilla `server.properties`, for whoever writes the server list file.
    [[nodiscard]] static std::optional<std::string>
    legacyMotd(const std::string& legacyPropertiesPath = "server.properties");
};

/// A `server.properties` as Java writes it: `key=value` or `key:value`, `#` and `!` for comments.
[[nodiscard]] std::map<std::string, std::string, std::less<>> parseJavaProperties(std::string_view text);

/// The default `server.toml` with the values a `server.properties` already answers written into it.
[[nodiscard]] std::string mergeLegacyProperties(std::string_view templateText,
                                                const std::map<std::string, std::string, std::less<>>& legacy);

} // namespace basalt::server::runtime::config
