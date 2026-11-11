#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/config/BasaltConfig.hpp>
#include <basalt/server/runtime/config/ServerListConfig.hpp>
#include <basalt/server/runtime/config/ServerTuning.hpp>
#include <basalt/server/runtime/plugins/ServerSettings.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <array>
#include <charconv>

namespace basalt::server::runtime::plugins {
namespace {

constexpr std::array<std::string_view, 40> kKeys{"motd",
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
                                                 "allow_end",
                                                 "pause_when_empty_seconds",
                                                 "spawn_protection",
                                                 "pause_allowed",
                                                 "respawn_world",
                                                 "autosave",
                                                 "rcon_enabled",
                                                 "rcon_port",
                                                 "api_enabled",
                                                 "api_port",
                                                 "management_enabled",
                                                 "management_host",
                                                 "management_port",
                                                 "management_tls",
                                                 "difficulty",
                                                 "enforce_allowlist",
                                                 "allow_flight",
                                                 "force_game_mode",
                                                 "accept_transfers",
                                                 "status_replies",
                                                 "entity_broadcast_range",
                                                 "status_heartbeat_interval",
                                                 "operator_user_permission_level"};

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

std::optional<std::string> ServerSettings::respawnWorldOverride() const {
    const std::lock_guard<std::mutex> lock{mutex_};
    return respawnWorld_;
}

std::int32_t ServerSettings::pauseWhenEmptySeconds() const {
    const std::int32_t held = pauseWhenEmpty_.load(std::memory_order_relaxed);
    return held >= 0 ? held : hooks_.config ? hooks_.config().pauseWhenEmptySeconds : 0;
}

std::optional<bool> ServerSettings::hidePlayersOverride() const {
    const std::int8_t held = hidePlayers_.load(std::memory_order_relaxed);
    return held < 0 ? std::nullopt : std::optional{held != 0};
}

std::int32_t ServerSettings::spawnProtection() const {
    const std::int32_t held = spawnProtection_.load(std::memory_order_relaxed);
    return held >= 0 ? held : hooks_.config ? hooks_.config().spawnProtection : 0;
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
    if (key == "pause_when_empty_seconds") {
        return std::to_string(pauseWhenEmptySeconds());
    }
    if (config::LiveTuning* live = hooks_.live ? hooks_.live() : nullptr; live != nullptr) {
        if (key == "enforce_allowlist") {
            return boolText(live->enforceWhitelist.load(std::memory_order_relaxed));
        }
        if (key == "allow_flight") {
            return boolText(live->allowFlight.load(std::memory_order_relaxed));
        }
        if (key == "force_game_mode") {
            return boolText(live->forceGamemode.load(std::memory_order_relaxed));
        }
        if (key == "accept_transfers") {
            return boolText(live->acceptsTransfers.load(std::memory_order_relaxed));
        }
        if (key == "status_replies") {
            return boolText(live->statusReplies.load(std::memory_order_relaxed));
        }
        if (key == "entity_broadcast_range") {
            return std::to_string(live->entityBroadcastRangePercentage.load(std::memory_order_relaxed));
        }
    }
    if (key == "status_heartbeat_interval") {
        return std::to_string(hooks_.heartbeatSeconds ? hooks_.heartbeatSeconds() : 0);
    }
    if (key == "operator_user_permission_level") {
        return std::to_string(command::permissionLevel::operatorLevel());
    }
    if (key == "difficulty") {
        const std::vector<world::LoadedWorld*> worlds =
            hooks_.worlds ? hooks_.worlds() : std::vector<world::LoadedWorld*>{};
        return worlds.empty() ? std::string{"normal"} : worlds.front()->difficulty();
    }
    if (key == "spawn_protection") {
        return std::to_string(spawnProtection());
    }
    if (key == "pause_allowed") {
        return boolText(pauseAllowed());
    }
    if (key == "autosave") {
        return boolText(!hooks_.autoSave || hooks_.autoSave());
    }
    if (key == "respawn_world") {
        if (const auto held = respawnWorldOverride(); held.has_value()) {
            return held;
        }
        const std::vector<world::LoadedWorld*> worlds =
            hooks_.worlds ? hooks_.worlds() : std::vector<world::LoadedWorld*>{};
        return worlds.empty() ? std::string{} : worlds.front()->name();
    }
    if (key == "port") {
        return std::to_string(config.port);
    }
    if (key == "rcon_enabled") {
        return boolText(config.remote.rcon.enabled && !config.remote.rcon.password.empty());
    }
    if (key == "rcon_port") {
        return std::to_string(config.remote.rcon.port);
    }
    if (key == "api_enabled") {
        return boolText(config.remote.api.enabled);
    }
    if (key == "api_port") {
        return std::to_string(config.remote.api.port);
    }
    if (key == "management_enabled") {
        return boolText(config.remote.management.enabled);
    }
    if (key == "management_host") {
        return config.remote.management.bind;
    }
    if (key == "management_port") {
        return std::to_string(config.remote.management.port);
    }
    if (key == "management_tls") {
        return boolText(config.remote.management.tls);
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
        return boolText(hooks_.whitelistOn ? hooks_.whitelistOn() : config.whitelist);
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
        if (worlds.empty()) {
            return std::string{"10"};
        }
        const world::LoadedWorld& first = *worlds.front();
        return std::to_string(key == "view_distance"
                                  ? first.settings().viewDistance
                                  : first.tunables().simulationDistance.value_or(first.settings().simulationDistance));
    }
    if (key == "hide_online_players") {
        if (const auto held = hidePlayersOverride(); held.has_value()) {
            return boolText(*held);
        }
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
    if (key == "autosave") {
        if ((value != "true" && value != "false") || !hooks_.setAutoSave) {
            return false;
        }
        hooks_.setAutoSave(value == "true");
        return true;
    }
    if (key == "pause_allowed") {
        if (value != "true" && value != "false") {
            return false;
        }
        pauseAllowed_.store(value == "true", std::memory_order_relaxed);
        return true;
    }
    if (key == "respawn_world") {
        for (world::LoadedWorld* world : hooks_.worlds ? hooks_.worlds() : std::vector<world::LoadedWorld*>{}) {
            if (world->name() == value) {
                const std::lock_guard<std::mutex> lock{mutex_};
                respawnWorld_ = std::string{value};
                return true;
            }
        }
        return false;
    }
    if (key == "pause_when_empty_seconds" || key == "spawn_protection") {
        const std::optional<std::int32_t> number = numberOf(value);
        if (!number.has_value() || *number < 0) {
            return false;
        }
        (key == "spawn_protection" ? spawnProtection_ : pauseWhenEmpty_).store(*number, std::memory_order_relaxed);
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
    if (const bool flag = value == "true"; value == "true" || value == "false") {
        config::LiveTuning* live = hooks_.live ? hooks_.live() : nullptr;
        if (key == "whitelist" && hooks_.setWhitelistOn) {
            hooks_.setWhitelistOn(flag);
            if (hooks_.kickUnlisted) {
                hooks_.kickUnlisted();
            }
            return true;
        }
        if (key == "hide_online_players") {
            hidePlayers_.store(flag ? 1 : 0, std::memory_order_relaxed);
            return true;
        }
        if (live != nullptr) {
            std::atomic<bool>* target = key == "enforce_allowlist"  ? &live->enforceWhitelist
                                        : key == "allow_flight"     ? &live->allowFlight
                                        : key == "force_game_mode"  ? &live->forceGamemode
                                        : key == "accept_transfers" ? &live->acceptsTransfers
                                        : key == "status_replies"   ? &live->statusReplies
                                                                    : nullptr;
            if (target != nullptr) {
                target->store(flag, std::memory_order_relaxed);
                if (key == "enforce_allowlist" && hooks_.kickUnlisted) {
                    hooks_.kickUnlisted();
                }
                return true;
            }
        }
    }
    if (key == "difficulty") {
        if (value != "peaceful" && value != "easy" && value != "normal" && value != "hard") {
            return false;
        }
        for (world::LoadedWorld* world : hooks_.worlds ? hooks_.worlds() : std::vector<world::LoadedWorld*>{}) {
            world->setDifficulty(std::string{value});
        }
        return true;
    }
    if (key == "entity_broadcast_range" || key == "status_heartbeat_interval" ||
        key == "operator_user_permission_level" || key == "view_distance" || key == "simulation_distance") {
        const std::optional<std::int32_t> number = numberOf(value);
        if (!number.has_value()) {
            return false;
        }
        return setNumber(key, *number);
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

bool ServerSettings::setNumber(const std::string_view key, const std::int32_t number) {
    const std::vector<world::LoadedWorld*> worlds =
        hooks_.worlds ? hooks_.worlds() : std::vector<world::LoadedWorld*>{};
    if (key == "entity_broadcast_range") {
        config::LiveTuning* live = hooks_.live ? hooks_.live() : nullptr;
        if (live == nullptr || number < 10 || number > 1000) {
            return false;
        }
        live->entityBroadcastRangePercentage.store(number, std::memory_order_relaxed);
        return true;
    }
    if (key == "status_heartbeat_interval") {
        if (number < 0 || !hooks_.setHeartbeatSeconds) {
            return false;
        }
        hooks_.setHeartbeatSeconds(number);
        return true;
    }
    if (key == "operator_user_permission_level") {
        if (number < 1 || number > 4) {
            return false;
        }
        command::permissionLevel::setOperatorLevel(number);
        return true;
    }
    if (key == "view_distance") {
        for (world::LoadedWorld* world : worlds) {
            world->setViewDistance(std::clamp(number, 2, 32));
        }
        return !worlds.empty();
    }
    for (world::LoadedWorld* world : worlds) {
        world->tunables().simulationDistance = std::clamp(number, 2, 32);
    }
    return !worlds.empty();
}

} // namespace basalt::server::runtime::plugins
