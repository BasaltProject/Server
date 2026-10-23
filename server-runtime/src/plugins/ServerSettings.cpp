#include <basalt/server/runtime/config/BasaltConfig.hpp>
#include <basalt/server/runtime/config/ServerListConfig.hpp>
#include <basalt/server/runtime/plugins/ServerSettings.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <array>
#include <charconv>

namespace basalt::server::runtime::plugins {
namespace {

constexpr std::array<std::string_view, 18> kKeys{"motd",
                                                 "max_players",
                                                 "port",
                                                 "bind",
                                                 "online_mode",
                                                 "enforce_secure_profile",
                                                 "whitelist",
                                                 "gamemode",
                                                 "idle_timeout",
                                                 "view_distance",
                                                 "simulation_distance",
                                                 "bedrock_port",
                                                 "hide_online_players",
                                                 "level_type",
                                                 "initial_enabled_packs",
                                                 "initial_disabled_packs",
                                                 "allow_nether",
                                                 "allow_end"};

[[nodiscard]] std::string joined(const std::vector<std::string>& values) {
    std::string out;
    for (const std::string& value : values) {
        out += out.empty() ? value : "," + value;
    }
    return out;
}

[[nodiscard]] std::string boolText(bool value) { return value ? "true" : "false"; }

[[nodiscard]] std::optional<std::int32_t> numberOf(std::string_view text) {
    std::int32_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size() ? std::optional{value} : std::nullopt;
}

} // namespace

std::vector<std::string> ServerSettings::keys() const { return {kKeys.begin(), kKeys.end()}; }

std::optional<std::string> ServerSettings::motdOverride() const {
    const std::lock_guard<std::mutex> lock{mutex_};
    return motd_;
}

std::optional<std::int32_t> ServerSettings::maxPlayersOverride() const {
    const std::lock_guard<std::mutex> lock{mutex_};
    return maxPlayers_;
}

std::optional<std::string> ServerSettings::get(const std::string_view key) const {
    if (!hooks_.config) {
        return std::nullopt;
    }
    const config::BasaltConfig& config = hooks_.config();
    if (key == "motd") {
        if (const auto override = motdOverride(); override.has_value()) {
            return override;
        }
        std::string joined;
        if (hooks_.serverList) {
            for (const std::string& line :
                 hooks_.serverList().defaultProfile().motd.value_or(std::vector<std::string>{})) {
                joined += joined.empty() ? line : "\n" + line;
            }
        }
        return joined;
    }
    if (key == "max_players") {
        return std::to_string(maxPlayersOverride().value_or(config.maxPlayers));
    }
    if (key == "port") {
        return std::to_string(config.port);
    }
    if (key == "bedrock_port") {
        return std::to_string(config.bedrockPort);
    }
    if (key == "bind") {
        return config.javaBind;
    }
    if (key == "online_mode") {
        return boolText(config.onlineMode);
    }
    if (key == "enforce_secure_profile") {
        return boolText(config.enforceSecureProfile);
    }
    if (key == "whitelist") {
        return boolText(config.whitelist);
    }
    if (key == "gamemode") {
        const std::vector<world::LoadedWorld*> worlds =
            hooks_.worlds ? hooks_.worlds() : std::vector<world::LoadedWorld*>{};
        if (!worlds.empty() && worlds.front()->defaultGamemode().has_value()) {
            return *worlds.front()->defaultGamemode();
        }
        return config.gamemode;
    }
    if (key == "idle_timeout") {
        return std::to_string(hooks_.idleTimeout ? hooks_.idleTimeout() : 0);
    }
    if (key == "view_distance" || key == "simulation_distance") {
        const std::vector<world::LoadedWorld*> worlds =
            hooks_.worlds ? hooks_.worlds() : std::vector<world::LoadedWorld*>{};
        return std::to_string(worlds.empty() ? 10 : worlds.front()->settings().viewDistance);
    }
    if (key == "hide_online_players") {
        return boolText(hooks_.serverList && hooks_.serverList().defaultProfile().hidePlayerCount.value_or(false));
    }
    if (key == "level_type") {
        const std::vector<world::LoadedWorld*> worlds =
            hooks_.worlds ? hooks_.worlds() : std::vector<world::LoadedWorld*>{};
        return "minecraft:" + std::string{config::worldTypeName(worlds.empty() ? config::WorldType::Normal
                                                                               : worlds.front()->declaration().type)};
    }
    if (key == "initial_enabled_packs") {
        return joined(config.initialEnabledPacks);
    }
    if (key == "initial_disabled_packs") {
        return joined(config.initialDisabledPacks);
    }
    if (key == "allow_nether" || key == "allow_end") {
        const config::WorldDimension wanted =
            key == "allow_nether" ? config::WorldDimension::Nether : config::WorldDimension::End;
        for (const world::LoadedWorld* world : hooks_.worlds ? hooks_.worlds() : std::vector<world::LoadedWorld*>{}) {
            if (world->declaration().dimension == wanted) {
                return boolText(true);
            }
        }
        return boolText(false);
    }
    return std::nullopt;
}

bool ServerSettings::set(const std::string_view key, const std::string_view value) {
    if (key == "motd") {
        const std::lock_guard<std::mutex> lock{mutex_};
        motd_ = std::string{value};
        return true;
    }
    if (key == "max_players") {
        const std::optional<std::int32_t> number = numberOf(value);
        if (!number.has_value() || *number < 0) {
            return false;
        }
        const std::lock_guard<std::mutex> lock{mutex_};
        maxPlayers_ = *number;
        return true;
    }
    if (key == "idle_timeout") {
        const std::optional<std::int32_t> number = numberOf(value);
        if (!number.has_value() || *number < 0 || !hooks_.setIdleTimeout) {
            return false;
        }
        hooks_.setIdleTimeout(*number);
        return true;
    }
    if (key == "gamemode") {
        if (value != "survival" && value != "creative" && value != "adventure" && value != "spectator") {
            return false;
        }
        for (world::LoadedWorld* world : hooks_.worlds ? hooks_.worlds() : std::vector<world::LoadedWorld*>{}) {
            world->setDefaultGamemode(std::string{value});
        }
        return true;
    }
    return false;
}

} // namespace basalt::server::runtime::plugins
