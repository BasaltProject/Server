#include "ManagementMethods.hpp"
#include "ManagementSchema.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>

namespace basalt::server::remote::managementMethods {
namespace {

namespace schema = managementSchema;

enum class Kind : std::uint8_t { Bool, Int, Text, Difficulty, GameMode, Permission };

struct Spec {
    std::string_view path;
    std::string_view getDescription;
    std::string_view setDescription;
    std::string_view getName;
    std::string_view setName;
    Kind kind;
};

constexpr std::array<std::string_view, 4> kDifficulties{"peaceful", "easy", "normal", "hard"};
constexpr std::array<std::string_view, 4> kGameModes{"survival", "creative", "adventure", "spectator"};

constexpr std::array<Spec, 20> kSpecs{{
    {"autosave", "Get whether automatic world saving is enabled on the server",
     "Enable or disable automatic world saving on the server", "enabled", "enable", Kind::Bool},
    {"difficulty", "Get the current difficulty level of the server", "Set the difficulty level of the server",
     "difficulty", "difficulty", Kind::Difficulty},
    {"enforce_allowlist",
     "Get whether allowlist enforcement is enabled (kicks players immediately when removed from allowlist)",
     "Enable or disable allowlist enforcement (when enabled, players are kicked immediately upon removal from "
     "allowlist)",
     "enforced", "enforce", Kind::Bool},
    {"use_allowlist", "Get whether the allowlist is enabled on the server",
     "Enable or disable the allowlist on the server (controls whether only allowlisted players can join)", "used",
     "use", Kind::Bool},
    {"max_players", "Get the maximum number of players allowed to connect to the server",
     "Set the maximum number of players allowed to connect to the server", "max", "max", Kind::Int},
    {"pause_when_empty_seconds",
     "Get the number of seconds before the game is automatically paused when no players are online",
     "Set the number of seconds before the game is automatically paused when no players are online", "seconds",
     "seconds", Kind::Int},
    {"player_idle_timeout", "Get the number of seconds before idle players are automatically kicked from the server",
     "Set the number of seconds before idle players are automatically kicked from the server", "seconds", "seconds",
     Kind::Int},
    {"allow_flight", "Get whether flight is allowed for players in Survival mode",
     "Allow or disallow flight for players in Survival mode", "allowed", "allow", Kind::Bool},
    {"motd", "Get the server's message of the day displayed to players",
     "Set the server's message of the day displayed to players", "message", "message", Kind::Text},
    {"spawn_protection_radius", "Get the spawn protection radius in blocks (only operators can edit within this area)",
     "Set the spawn protection radius in blocks (only operators can edit within this area)", "radius", "radius",
     Kind::Int},
    {"force_game_mode", "Get whether players are forced to use the server's default game mode",
     "Enable or disable forcing players to use the server's default game mode", "forced", "force", Kind::Bool},
    {"game_mode", "Get the server's default game mode", "Set the server's default game mode", "mode", "mode",
     Kind::GameMode},
    {"view_distance", "Get the server's view distance in chunks", "Set the server's view distance in chunks",
     "distance", "distance", Kind::Int},
    {"simulation_distance", "Get the server's simulation distance in chunks",
     "Set the server's simulation distance in chunks", "distance", "distance", Kind::Int},
    {"accept_transfers", "Get whether the server accepts player transfers from other servers",
     "Enable or disable accepting player transfers from other servers", "accepted", "accept", Kind::Bool},
    {"status_heartbeat_interval", "Get the interval in seconds between server status heartbeats",
     "Set the interval in seconds between server status heartbeats", "seconds", "seconds", Kind::Int},
    {"operator_user_permission_level", "Get default operator permission level", "Set default operator permission level",
     "level", "level", Kind::Permission},
    {"hide_online_players", "Get whether the server hides online player information from status queries",
     "Enable or disable hiding online player information from status queries", "hidden", "hide", Kind::Bool},
    {"status_replies", "Get whether the server responds to connection status requests",
     "Enable or disable the server responding to connection status requests", "enabled", "enable", Kind::Bool},
    {"entity_broadcast_range", "Get the entity broadcast range as a percentage",
     "Set the entity broadcast range as a percentage", "percentage_points", "percentage_points", Kind::Int},
}};

[[nodiscard]] JsonValue schemaOf(const Kind kind) {
    switch (kind) {
    case Kind::Bool:
        return schema::type("boolean");
    case Kind::Int:
    case Kind::Permission:
        return schema::type("integer");
    case Kind::Text:
        return schema::type("string");
    case Kind::Difficulty:
        return schema::ref("difficulty");
    case Kind::GameMode:
        return schema::ref("game_type");
    }
    return {};
}

[[nodiscard]] Outcome encode(const Kind kind, const std::string& text) {
    if (kind == Kind::Bool) {
        return JsonValue::of(text == "true");
    }
    if (kind == Kind::Int || kind == Kind::Permission) {
        std::int64_t number = 0;
        std::from_chars(text.data(), text.data() + text.size(), number);
        return JsonValue::of(number);
    }
    return JsonValue::of(text);
}

[[nodiscard]] bool isOneOf(const std::string& text, const std::array<std::string_view, 4>& names) {
    return std::ranges::find(names, text) != names.end();
}

/// The setting as the backend takes it, or the reason the JSON value cannot be one.
[[nodiscard]] std::expected<std::string, std::string> decode(const Kind kind, const JsonValue& value) {
    switch (kind) {
    case Kind::Bool:
        if (const auto* flag = value.get<bool>(); flag != nullptr) {
            return std::string{*flag ? "true" : "false"};
        }
        return std::unexpected{std::string{"Not a boolean"}};
    case Kind::Int:
    case Kind::Permission: {
        const auto* number = value.get<std::int64_t>();
        if (number == nullptr || *number < std::numeric_limits<std::int32_t>::min() ||
            *number > std::numeric_limits<std::int32_t>::max()) {
            return std::unexpected{std::string{"Not an integer"}};
        }
        if (kind == Kind::Permission && (*number < 0 || *number > 4)) {
            return std::unexpected{std::string{"Value must be within range [0;4]"}};
        }
        return std::to_string(*number);
    }
    case Kind::Text:
        if (const auto* text = value.get<std::string>(); text != nullptr) {
            return *text;
        }
        return std::unexpected{std::string{"Not a string"}};
    case Kind::Difficulty:
    case Kind::GameMode: {
        const auto* text = value.get<std::string>();
        if (text == nullptr || !isOneOf(*text, kind == Kind::Difficulty ? kDifficulties : kGameModes)) {
            return std::unexpected{std::string{"Unknown value, expected one of the names in the schema"}};
        }
        return *text;
    }
    }
    return std::unexpected{std::string{"Unsupported"}};
}

[[nodiscard]] Outcome current(ManagementBackend& backend, const Spec& spec) {
    const std::optional<std::string> held = backend.setting(spec.path);
    if (!held.has_value()) {
        return std::unexpected{Failure{FailureKind::Internal, "The server has no such setting"}};
    }
    return encode(spec.kind, *held);
}

} // namespace

void addSettingsMethods(std::vector<IncomingMethod>& out) {
    for (const Spec& spec : kSpecs) {
        const std::string path = "serversettings/" + std::string{spec.path};
        out.push_back(IncomingMethod{.name = path,
                                     .description = std::string{spec.getDescription},
                                     .paramName = "",
                                     .paramSchema = {},
                                     .resultName = std::string{spec.getName},
                                     .resultSchema = schemaOf(spec.kind),
                                     .discoverable = true,
                                     .handler = [&spec](ManagementBackend& backend, const JsonValue*) -> Outcome {
                                         return current(backend, spec);
                                     }});
        out.push_back(IncomingMethod{.name = path + "/set",
                                     .description = std::string{spec.setDescription},
                                     .paramName = std::string{spec.setName},
                                     .paramSchema = schemaOf(spec.kind),
                                     .resultName = std::string{spec.getName},
                                     .resultSchema = schemaOf(spec.kind),
                                     .discoverable = true,
                                     .handler = [&spec](ManagementBackend& backend, const JsonValue* param) -> Outcome {
                                         const auto text = decode(spec.kind, *param);
                                         if (!text.has_value()) {
                                             return std::unexpected{invalid(text.error())};
                                         }
                                         if (const std::optional<std::string> refused =
                                                 backend.changeSetting(spec.path, *text);
                                             refused.has_value()) {
                                             return std::unexpected{invalid(*refused)};
                                         }
                                         return current(backend, spec);
                                     }});
    }
}

} // namespace basalt::server::remote::managementMethods
