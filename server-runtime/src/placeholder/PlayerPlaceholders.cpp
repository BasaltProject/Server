#include <basalt/server/runtime/config/GroupsConfig.hpp>
#include <basalt/server/runtime/config/ServerListConfig.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/placeholder/Decimals.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/server/runtime/placeholder/PlayerPlaceholders.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace basalt::server::runtime::placeholder {
namespace {

using config::PingRequest;
using player::PlayerSession;
using wire::common::placeholder::PlaceholderValue;

constexpr std::string_view kNoSubject = "this text is not about any player";
constexpr std::string_view kStatPrefix = "stat_";
constexpr std::string_view kPermissionPrefix = "has_permission_";
constexpr std::string_view kNotRecognised =
    "nobody has connected from that address before, so there is no name to greet. "
    "Put the greeting in a profile with when = [\"%basalt_player_known%\"]";

constexpr std::array<std::string_view, 16> kCompass{"S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW",
                                                    "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE"};

std::string_view modeName(wire::protocol::play::Gamemode mode) {
    switch (mode) {
    case wire::protocol::play::Gamemode::Creative:
        return "creative";
    case wire::protocol::play::Gamemode::Adventure:
        return "adventure";
    case wire::protocol::play::Gamemode::Spectator:
        return "spectator";
    case wire::protocol::play::Gamemode::Survival:
    default:
        return "survival";
    }
}

PlaceholderValue needsPlayer(const PingRequest* ping) {
    return PlaceholderValue::unavailable(
        ping == nullptr ? std::string{kNoSubject}
                        : std::string{"there is no player yet: this is a server list ping, before login"});
}

/// A name a ping *could* have answered and did not, which is not the same as having no subject.
///
/// `%basalt_player_name%` in the server list is a fair question: it is asking who is looking. It
/// simply has no answer for somebody this server has never seen, and saying "this text is not about
/// any player" there was plainly wrong, since it is about the person pinging.
PlaceholderValue unanswered(const PingRequest* ping) {
    return PlaceholderValue::unavailable(ping == nullptr ? std::string{kNoSubject} : std::string{kNotRecognised});
}

/// Answerable from a pending connection as well as from a player: what a handshake already carries.
std::optional<PlaceholderValue> eitherWay(std::string_view params, const PlayerSession* session,
                                          const PingRequest* ping) {
    const auto text = [](std::string value) { return std::optional{PlaceholderValue::of(std::move(value))}; };
    if (params == "protocol") {
        if (ping != nullptr) {
            return PlaceholderValue::of(static_cast<std::int64_t>(ping->protocolVersion()));
        }
        return session == nullptr
                   ? std::nullopt
                   : std::optional{PlaceholderValue::of(static_cast<std::int64_t>(session->protocolVersion()))};
    }
    if (params == "host") {
        if (ping != nullptr) {
            return text(ping->host());
        }
        return session == nullptr ? std::nullopt : text(session->connectedHost());
    }
    if (params == "ip") {
        if (ping != nullptr && ping->address().has_value()) {
            return text(*ping->address());
        }
        return session != nullptr && session->address().has_value() ? text(*session->address()) : std::nullopt;
    }
    if (params == "name" || params == "displayname" || params == "list_name") {
        if (session != nullptr) {
            return text(session->name());
        }
        if (ping != nullptr && ping->knownPlayer().has_value()) {
            return text(*ping->knownPlayer());
        }
        return std::nullopt;
    }
    if (params == "known") {
        if (session != nullptr) {
            return PlaceholderValue::of(true);
        }
        return ping == nullptr ? std::nullopt : std::optional{PlaceholderValue::of(ping->knownPlayer().has_value())};
    }
    return std::nullopt;
}

std::optional<PlaceholderValue> ofPlayer(std::string_view params, const PlayerSession& player,
                                         const PlayerPlaceholderSources& sources) {
    if (params == "uuid") {
        return PlaceholderValue::of(player.uuid().toString());
    }
    if (params == "online") {
        return PlaceholderValue::of(true);
    }
    if (params == "world") {
        return PlaceholderValue::of(player.worldName);
    }
    if (params == "dimension") {
        const world::LoadedWorld* world = sources.world ? sources.world(player.worldName) : nullptr;
        return world == nullptr ? PlaceholderValue::unavailable()
                                : PlaceholderValue::of(
                                      std::string{config::dimensionFacts(world->declaration().dimension).configName});
    }
    if (params == "x") {
        return PlaceholderValue::of(player.x);
    }
    if (params == "y") {
        return PlaceholderValue::of(player.y);
    }
    if (params == "z") {
        return PlaceholderValue::of(player.z);
    }
    if (params == "yaw") {
        return PlaceholderValue::of(static_cast<double>(player.yaw));
    }
    if (params == "pitch") {
        return PlaceholderValue::of(static_cast<double>(player.pitch));
    }
    if (params == "direction") {
        return PlaceholderValue::of(std::string{compassDirection(player.yaw)});
    }
    if (params == "direction_xz") {
        return PlaceholderValue::of(std::string{compassDirectionXZ(player.yaw)});
    }
    if (params == "gamemode") {
        return PlaceholderValue::of(std::string{modeName(player.gamemode)});
    }
    if (params == "group") {
        return PlaceholderValue::of(sources.groups().get(player.group).name);
    }
    if (params == "is_op") {
        return PlaceholderValue::of(sources.groups().get(player.group).isOperator);
    }
    if (params == "health") {
        return PlaceholderValue::of(static_cast<double>(player.health()));
    }
    if (params == "health_rounded") {
        return PlaceholderValue::of(static_cast<std::int64_t>(std::lround(player.health())));
    }
    if (params == "max_health") {
        return PlaceholderValue::of(static_cast<double>(PlayerSession::kMaxHealth));
    }
    if (params == "max_health_rounded") {
        return PlaceholderValue::of(static_cast<std::int64_t>(std::lround(PlayerSession::kMaxHealth)));
    }
    if (params == "food_level") {
        return PlaceholderValue::of(static_cast<std::int64_t>(player.food.foodLevel()));
    }
    if (params == "saturation") {
        return PlaceholderValue::of(static_cast<double>(player.food.saturationLevel()));
    }
    if (params == "level") {
        return PlaceholderValue::of(static_cast<std::int64_t>(player.experience.level()));
    }
    if (params == "exp") {
        return PlaceholderValue::of(static_cast<double>(player.experience.progress()));
    }
    if (params == "current_exp") {
        return PlaceholderValue::of(static_cast<std::int64_t>(
            player.experience.progress() * static_cast<float>(player.experience.xpNeededForNextLevel())));
    }
    if (params == "exp_to_level") {
        return PlaceholderValue::of(static_cast<std::int64_t>(player.experience.xpNeededForNextLevel()));
    }
    if (params == "total_exp") {
        return PlaceholderValue::of(static_cast<std::int64_t>(player.experience.total()));
    }
    if (params == "is_flying") {
        return PlaceholderValue::of(player.flying);
    }
    if (params == "allow_flight") {
        return PlaceholderValue::of(player.mayFly());
    }
    if (params == "is_sprinting") {
        return PlaceholderValue::of(player.sprinting);
    }
    if (params == "is_dead") {
        return PlaceholderValue::of(player.dead);
    }
    return std::nullopt;
}

/// The names only a logged-in player can answer.
const std::vector<std::string>& playerNames() {
    static const std::vector<std::string> kNames{"uuid",
                                                 "online",
                                                 "world",
                                                 "dimension",
                                                 "x",
                                                 "y",
                                                 "z",
                                                 "yaw",
                                                 "pitch",
                                                 "direction",
                                                 "direction_xz",
                                                 "gamemode",
                                                 "group",
                                                 "is_op",
                                                 "health",
                                                 "health_rounded",
                                                 "max_health",
                                                 "max_health_rounded",
                                                 "food_level",
                                                 "saturation",
                                                 "level",
                                                 "exp",
                                                 "current_exp",
                                                 "exp_to_level",
                                                 "total_exp",
                                                 "is_flying",
                                                 "allow_flight",
                                                 "is_sprinting",
                                                 "is_dead"};
    return kNames;
}

bool isPlayerName(std::string_view params) { return std::ranges::find(playerNames(), params) != playerNames().end(); }

/// The names a handshake already carries, which a server list ping can answer too.
const std::vector<std::string>& sharedNames() {
    static const std::vector<std::string> kNames{"protocol", "host", "ip", "name", "displayname", "list_name", "known"};
    return kNames;
}

bool isSharedName(std::string_view params) { return std::ranges::find(sharedNames(), params) != sharedNames().end(); }

/// Placeholders whose meaning is settled but whose answer needs a system this build has not written
/// yet. Registered so that using one is reported as "not yet" and not as a typo.
const std::map<std::string, std::string, std::less<>>& notImplementedReasons() {
    static const std::map<std::string, std::string, std::less<>> kReasons = [] {
        std::map<std::string, std::string, std::less<>> reasons;
        const std::string inventory = "there is no inventory system yet";
        for (const std::string_view name :
             {"item_in_hand", "item_in_hand_name", "item_in_hand_data", "item_in_hand_durability", "item_in_offhand",
              "item_in_offhand_name", "item_in_offhand_data", "item_in_offhand_durability", "has_empty_slot",
              "empty_slots", "can_pickup_items"}) {
            reasons.emplace(name, inventory);
        }
        for (const std::string_view piece : {"helmet", "chestplate", "leggings", "boots"}) {
            for (const std::string_view suffix : {"name", "data", "durability"}) {
                reasons.emplace("armor_" + std::string{piece} + "_" + std::string{suffix}, inventory);
            }
        }
        for (const std::string_view name : {"has_health_boost", "health_boost", "absorption", "last_damage"}) {
            reasons.emplace(name, "there are no potion effects or damage tracking yet");
        }
        for (const std::string_view name : {"bed_x", "bed_y", "bed_z", "bed_world", "is_sleeping", "sleep_ticks"}) {
            reasons.emplace(name, "beds and respawn points are not implemented yet");
        }
        for (const std::string_view name : {"compass_world", "compass_x", "compass_y", "compass_z"}) {
            reasons.emplace(name, "there is no compass target yet");
        }
        for (const std::string_view name : {"biome", "biome_capitalized", "block_underneath", "light_level"}) {
            reasons.emplace(name, "the world lookup a live player needs for this is not wired up yet");
        }
        for (const std::string_view name :
             {"locale", "locale_display_name", "locale_short", "locale_country", "locale_display_country"}) {
            reasons.emplace(name, "the client's language is received but not kept on the session yet");
        }
        for (const std::string_view name : {"ping", "colored_ping"}) {
            reasons.emplace(name, "round-trip time is not measured yet");
        }
        for (const std::string_view name : {"first_join_date", "first_played", "first_join", "first_played_formatted",
                                            "last_played", "last_join", "last_played_formatted", "last_join_date",
                                            "has_played_before", "minutes_lived", "seconds_lived", "ticks_lived"}) {
            reasons.emplace(name, "the stored row is not kept on the live session yet");
        }
        for (const std::string_view name :
             {"is_whitelisted", "is_banned", "is_sneaking", "is_inside_vehicle", "custom_name", "fly_speed",
              "walk_speed", "health_scale", "max_air", "remaining_air", "max_no_damage_ticks", "no_damage_ticks",
              "thunder_duration", "weather_duration", "time", "time_offset", "world_type", "world_time_12",
              "world_time_24"}) {
            reasons.emplace(name, "not implemented yet");
        }
        return reasons;
    }();
    return kReasons;
}

} // namespace

std::string_view compassDirection(float yaw) {
    const float wrapped = std::fmod(std::fmod(yaw, 360.0F) + 360.0F, 360.0F);
    return kCompass[static_cast<std::size_t>(static_cast<std::int32_t>((wrapped / 22.5F) + 0.5F) % 16)];
}

std::string_view compassDirectionXZ(float yaw) {
    const float wrapped = std::fmod(std::fmod(yaw, 360.0F) + 360.0F, 360.0F);
    return kCompass[static_cast<std::size_t>((static_cast<std::int32_t>((wrapped / 90.0F) + 0.5F) % 4) * 4)];
}

std::string expandStatKey(std::string_view key) {
    if (key.find(':') != std::string_view::npos) {
        return std::string{key};
    }
    const std::string_view category = storage::stats::kMined.substr(storage::stats::kMined.find(':') + 1);
    const std::string prefix = std::string{category} + "_";
    if (key.starts_with(prefix)) {
        return storage::stats::mined("minecraft:" + std::string{key.substr(prefix.size())});
    }
    return storage::stats::custom(key);
}

wire::common::placeholder::PlaceholderProvider playerPlaceholders(PlayerPlaceholderSources sources) {
    return
        [sources = std::move(sources)](
            std::string_view params, const wire::common::placeholder::PlaceholderContext& context) -> PlaceholderValue {
            const PlayerSession* session = context.get(playerSlot());
            const PingRequest* ping = context.get(pingSlot());

            if (isSharedName(params)) {
                const std::optional<PlaceholderValue> shared = eitherWay(params, session, ping);
                return shared.has_value() ? *shared : unanswered(ping);
            }
            const auto reason = notImplementedReasons().find(params);
            if (reason != notImplementedReasons().end()) {
                return notImplemented(reason->second);
            }
            if (params.starts_with(kStatPrefix) && params.size() > kStatPrefix.size()) {
                // Written without the namespaces for readability: `stat_mined_stone` rather than the
                // full `minecraft:mined/minecraft:stone` a file would otherwise have to spell out.
                if (session == nullptr) {
                    return needsPlayer(ping);
                }
                return PlaceholderValue::of(session->stats.get(expandStatKey(params.substr(kStatPrefix.size()))));
            }
            if (params.starts_with(kPermissionPrefix) && params.size() > kPermissionPrefix.size()) {
                if (session == nullptr) {
                    return needsPlayer(ping);
                }
                return PlaceholderValue::of(
                    sources.groups().resolve(session->group, params.substr(kPermissionPrefix.size())));
            }
            if (isPlayerName(params)) {
                if (session == nullptr) {
                    return needsPlayer(ping);
                }
                return ofPlayer(params, *session, sources).value_or(PlaceholderValue::unknown());
            }
            // `x_1` and friends: the same placeholder, rounded. Tried last, so a real name that happens
            // to end in a digit is never mistaken for a precision.
            const std::optional<std::pair<std::string, std::int32_t>> rounded = decimals::split(params);
            if (!rounded.has_value() || !isPlayerName(rounded->first)) {
                return PlaceholderValue::unknown();
            }
            if (session == nullptr) {
                return needsPlayer(ping);
            }
            const std::optional<PlaceholderValue> answer = ofPlayer(rounded->first, *session, sources);
            const std::string* text = answer.has_value() ? answer->textOrNull() : nullptr;
            if (text == nullptr) {
                return PlaceholderValue::unknown();
            }
            double value = 0.0;
            const char* end = text->data() + text->size();
            const std::from_chars_result parsed = std::from_chars(text->data(), end, value);
            if (parsed.ec != std::errc{} || parsed.ptr != end) {
                return PlaceholderValue::unknown();
            }
            return PlaceholderValue::of(decimals::format(value, rounded->second));
        };
}

wire::common::placeholder::PlaceholderCatalog playerPlaceholderNames() {
    return [] {
        std::vector<std::string> names = sharedNames();
        names.insert(names.end(), playerNames().begin(), playerNames().end());
        for (const auto& [name, reason] : notImplementedReasons()) {
            names.push_back(name);
        }
        names.emplace_back(std::string{kPermissionPrefix} + "<node>");
        names.emplace_back(std::string{kStatPrefix} + "<name>");
        names.emplace_back("<any number>_<decimals>");
        std::ranges::sort(names);
        return names;
    };
}

} // namespace basalt::server::runtime::placeholder
