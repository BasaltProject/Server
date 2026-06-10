#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/server/runtime/config/ServerListConfig.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::config {
namespace {

using wire::common::toml::TomlTable;

constexpr std::string_view kModernMotdBlock =
    R"(motd = [
    "              <bold><gradient:gray:white>BASALT SERVER</gradient></bold> <gray>[1.8-26.3]</gray>",
    "              <white>Welcome to your new server</white>",
])";

constexpr std::string_view kLegacyMotdBlock =
    R"(motd = [
    "              <bold><gray>BASALT SERVER</gray></bold> <dark_gray>[1.8-26.3]</dark_gray>",
    "              <white>Welcome to your new server</white>",
])";

[[nodiscard]] std::string escapeToml(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (const char character : text) {
        if (character == '\\' || character == '"') {
            escaped += '\\';
        }
        escaped += character;
    }
    return escaped;
}

[[nodiscard]] bool isBlank(std::string_view text) {
    return std::ranges::all_of(text, [](char character) { return static_cast<unsigned char>(character) <= ' '; });
}

void replaceAll(std::string& text, std::string_view what, std::string_view with) {
    std::size_t at = text.find(what);
    while (at != std::string::npos) {
        text.replace(at, what.size(), with);
        at = text.find(what, at + with.size());
    }
}

} // namespace

const std::optional<std::string>& PingRequest::knownPlayer() const {
    if (!asked_) {
        asked_ = true;
        if (lookup_) {
            knownPlayer_ = lookup_();
        }
    }
    return knownPlayer_;
}

ServerListProfile ServerListProfile::from(ProfileHeader header, const TomlTable& table) {
    const auto text = [&table](std::string_view key) -> std::optional<std::string> {
        const std::optional<std::string_view> written = table.string(key);
        return written.has_value() ? std::optional<std::string>{std::string{*written}} : std::nullopt;
    };
    const auto number = [&table](std::string_view key) -> std::optional<std::int32_t> {
        const std::optional<std::int64_t> written = table.integer(key);
        return written.has_value() ? std::optional<std::int32_t>{static_cast<std::int32_t>(*written)} : std::nullopt;
    };
    std::optional<std::vector<std::string>> hover;
    if (std::vector<std::string> written = table.stringList("hover"); !written.empty()) {
        hover = std::move(written);
    }
    return ServerListProfile{.header = std::move(header),
                             .motd = linesOf(table, "motd"),
                             .versionName = text("version-name"),
                             .versionProtocol = number("version-protocol"),
                             .hidePlayerCount = table.boolean("hide-player-count"),
                             .maxPlayers = number("max-players"),
                             .onlinePlayers = number("online-players"),
                             .hover = std::move(hover),
                             .icon = text("icon")};
}

ServerListProfile ServerListProfile::inheriting(const ServerListProfile& fallback) const {
    if (&fallback == this) {
        return *this;
    }
    ServerListProfile filled = *this;
    if (!filled.motd.has_value()) {
        filled.motd = fallback.motd;
    }
    if (!filled.versionName.has_value()) {
        filled.versionName = fallback.versionName;
    }
    if (!filled.versionProtocol.has_value()) {
        filled.versionProtocol = fallback.versionProtocol;
    }
    if (!filled.hidePlayerCount.has_value()) {
        filled.hidePlayerCount = fallback.hidePlayerCount;
    }
    if (!filled.maxPlayers.has_value()) {
        filled.maxPlayers = fallback.maxPlayers;
    }
    if (!filled.onlinePlayers.has_value()) {
        filled.onlinePlayers = fallback.onlinePlayers;
    }
    if (!filled.hover.has_value()) {
        filled.hover = fallback.hover;
    }
    if (!filled.icon.has_value()) {
        filled.icon = fallback.icon;
    }
    return filled;
}

ServerListProfile ServerListConfig::select(const wire::common::placeholder::PlaceholderRegistry& registry,
                                           const wire::common::placeholder::PlaceholderContext& context,
                                           const wire::common::Logger& logger) const {
    const ServerListProfile& chosen =
        selectProfile(profiles_, defaultProfile(), registry, context, logger, *reporter_, "server list");
    return chosen.inheriting(defaultProfile());
}

std::string serverListDefaultContents(const std::optional<std::string>& legacyMotd,
                                      const wire::common::Logger& logger) {
    if (!legacyMotd.has_value() || isBlank(*legacyMotd)) {
        return std::string{kServerListDefaultToml};
    }
    if (legacyMotd->find("§") != std::string::npos) {
        logger.warn([] {
            return std::string{"The MOTD imported from server.properties uses section-sign colour codes, which "
                               "are not translated: rewrite it in MiniMessage in "} +
                   std::string{ServerListConfig::kFileName} + ".";
        });
    }

    std::string text = *legacyMotd;
    replaceAll(text, "\\n", "\n");
    std::string lines = "[\n";
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::string_view line = end == std::string::npos ? std::string_view{text}.substr(start)
                                                               : std::string_view{text}.substr(start, end - start);
        lines += "    \"" + escapeToml(line) + "\",\n";
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    lines += "]";

    std::string contents{kServerListDefaultToml};
    replaceAll(contents, kModernMotdBlock, "motd = " + lines);
    replaceAll(contents, kLegacyMotdBlock, "motd = " + lines);
    return contents;
}

std::expected<ServerListConfig, std::string> ServerListConfig::load(const ConfigDirectory& directory,
                                                                    const std::optional<std::string>& legacyMotd,
                                                                    const wire::common::Logger& logger) {
    const std::string path = directory.pathOf(kFileName);
    const std::expected<TomlTable, std::string> root =
        directory.load(kFileName, serverListDefaultContents(legacyMotd, logger));
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }
    auto read = parseProfiles(*root, path, [](ProfileHeader header, const TomlTable& table) {
        return ServerListProfile::from(std::move(header), table);
    });
    if (!read.has_value()) {
        return std::unexpected(read.error());
    }

    // Named at startup: a server showing its maintenance message because a flag was left on is hard to debug.
    const ServerListProfile* active = nullptr;
    for (const ServerListProfile& profile : read->first) {
        if (profile.header.isActive && (active == nullptr || profile.header.priority > active->header.priority)) {
            active = &profile;
        }
    }
    if (active != nullptr) {
        const std::string name = active->header.name;
        logger.info([&path, name] { return "Server list profile '" + name + "' is switched on in " + path + "."; });
    }

    const TomlTable* greeting = root->table("greeting");
    const bool rememberIp = greeting == nullptr ? false : greeting->boolean("remember-ip").value_or(false);
    return ServerListConfig{std::move(read->first), read->second, rememberIp};
}

} // namespace basalt::server::runtime::config
