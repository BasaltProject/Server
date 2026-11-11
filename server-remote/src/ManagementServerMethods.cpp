#include "ManagementDto.hpp"
#include "ManagementMethods.hpp"
#include "ManagementSchema.hpp"

#include <algorithm>

namespace basalt::server::remote::managementMethods {
namespace {

using namespace managementDto;
namespace schema = managementSchema;

void add(std::vector<IncomingMethod>& out, std::string name, std::string description, std::string paramName,
         JsonValue paramSchema, std::string resultName, JsonValue resultSchema, Handler handler) {
    out.push_back(IncomingMethod{.name = std::move(name),
                                 .description = std::move(description),
                                 .paramName = std::move(paramName),
                                 .paramSchema = std::move(paramSchema),
                                 .resultName = std::move(resultName),
                                 .resultSchema = std::move(resultSchema),
                                 .discoverable = true,
                                 .handler = std::move(handler)});
}

struct SystemMessage {
    ManagedMessage message;
    bool overlay = false;
    std::optional<std::vector<ManagedPlayer>> receivers;
};

[[nodiscard]] Parsed<SystemMessage> parseSystemMessage(const JsonValue& value) {
    const auto* object = value.get<wire::common::JsonObject>();
    const JsonValue* message = object == nullptr ? nullptr : object->find("message");
    const JsonValue* overlay = object == nullptr ? nullptr : object->find("overlay");
    if (message == nullptr || overlay == nullptr || overlay->get<bool>() == nullptr) {
        return std::unexpected{std::string{"A system message needs a message and a boolean overlay"}};
    }
    auto parsedMessage = parseMessage(*message);
    if (!parsedMessage.has_value()) {
        return std::unexpected{parsedMessage.error()};
    }
    SystemMessage out{std::move(*parsedMessage), *overlay->get<bool>(), std::nullopt};
    if (const JsonValue* receivers = object->find("receivingPlayers"); receivers != nullptr) {
        auto list = parseList<ManagedPlayer>(*receivers, parsePlayer);
        if (list.has_value()) {
            out.receivers = std::move(*list);
        }
    }
    return out;
}

[[nodiscard]] JsonValue stateNow(ManagementBackend& backend) {
    if (!backend.ready()) {
        return encodeServerState(false, {}, backend.versionName(), backend.protocolVersion());
    }
    return encodeServerState(true, backend.onlinePlayers(), backend.versionName(), backend.protocolVersion());
}

[[nodiscard]] JsonValue rulesNow(ManagementBackend& backend) {
    std::vector<JsonValue> rules;
    for (const ManagedGameRule& rule : backend.gameRules()) {
        rules.push_back(encodeTypedRule(rule));
    }
    return JsonValue::ofArray(std::move(rules));
}

void addServerState(std::vector<IncomingMethod>& out) {
    const JsonValue flag = schema::type("boolean");
    add(out, "server/status", "Get server status", "", {}, "status", schema::ref("server_state"),
        [](ManagementBackend& backend, const JsonValue*) -> Outcome { return stateNow(backend); });
    add(out, "server/save", "Save server state", "flush", flag, "saving", flag,
        [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto* flush = param->get<bool>();
            if (flush == nullptr) {
                return std::unexpected{invalid("Not a boolean: flush")};
            }
            return JsonValue::of(backend.save(*flush));
        });
    add(out, "server/stop", "Stop server", "", {}, "stopping", flag,
        [](ManagementBackend& backend, const JsonValue*) -> Outcome {
            backend.stop();
            return JsonValue::of(true);
        });
    add(out, "server/system_message", "Send a system message", "message", schema::ref("system_message"), "sent", flag,
        [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto parsed = parseSystemMessage(*param);
            if (!parsed.has_value()) {
                return std::unexpected{invalid(parsed.error())};
            }
            const ManagedMessage& message = parsed->message;
            if (!message.translatable.has_value() && !message.literal.has_value()) {
                return JsonValue::of(false);
            }
            if (parsed->receivers.has_value() && parsed->receivers->empty()) {
                return JsonValue::of(false);
            }
            return JsonValue::of(backend.systemMessage(message, parsed->overlay, parsed->receivers));
        });
}

void addGameRules(std::vector<IncomingMethod>& out) {
    add(out, "gamerules", "Get the available game rule keys and their current values", "", {}, "gamerules",
        schema::array(schema::ref("typed_game_rule")),
        [](ManagementBackend& backend, const JsonValue*) -> Outcome { return rulesNow(backend); });
    add(out, "gamerules/update", "Update game rule value", "gamerule", schema::ref("untyped_game_rule"), "gamerule",
        schema::ref("typed_game_rule"), [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto parsed = parseUntypedRule(*param);
            if (!parsed.has_value()) {
                return std::unexpected{invalid(parsed.error())};
            }
            if (const std::optional<std::string> refused = backend.updateGameRule(*parsed); refused.has_value()) {
                return std::unexpected{invalid(*refused)};
            }
            for (const ManagedGameRule& rule : backend.gameRules()) {
                if (rule.key == parsed->key) {
                    return encodeTypedRule(rule);
                }
            }
            return std::unexpected{invalid("Unknown game rule: " + parsed->key)};
        });
}

} // namespace

void addServerMethods(std::vector<IncomingMethod>& out) {
    addServerState(out);
    addSettingsMethods(out);
    addGameRules(out);
}

const std::vector<IncomingMethod>& incomingMethods() {
    static const std::vector<IncomingMethod> methods = [] {
        std::vector<IncomingMethod> out;
        addListMethods(out);
        addServerMethods(out);
        out.push_back(IncomingMethod{
            .name = "rpc.discover",
            .description = "",
            .paramName = "",
            .paramSchema = {},
            .resultName = "result",
            .resultSchema = schema::type("string"),
            .discoverable = false,
            .handler = [](ManagementBackend&, const JsonValue*) -> Outcome { return schema::discoverDocument(); }});
        return out;
    }();
    return methods;
}

const std::vector<OutgoingMethod>& outgoingMethods() {
    static const std::vector<OutgoingMethod> methods = [] {
        const JsonValue player = schema::ref("player");
        const auto notification = [](std::string name, std::string description, std::string param = {},
                                     JsonValue paramSchema = {}) {
            return OutgoingMethod{std::move(name), std::move(description), std::move(param), std::move(paramSchema)};
        };
        return std::vector<OutgoingMethod>{
            notification("server/started", "Server started"),
            notification("server/stopping", "Server shutting down"),
            notification("server/saving", "Server save started"),
            notification("server/saved", "Server save completed"),
            notification("server/activity", "Server activity occurred. Rate limited to 1 notification per 30 seconds"),
            notification("world/upgrade_started", "World upgrade started"),
            notification("world/upgrade_progress", "World upgrade progress. Rate limited to 1 notification per second",
                         "progress", schema::type("number")),
            notification("world/upgrade_finished", "World upgrade finished"),
            notification("world/upgrade_failed", "World upgrade failed", "reason", schema::type("string")),
            notification("players/joined", "Player joined", "player", player),
            notification("players/left", "Player left", "player", player),
            notification("operators/added", "Player was oped", "player", schema::ref("operator")),
            notification("operators/removed", "Player was deoped", "player", schema::ref("operator")),
            notification("allowlist/added", "Player was added to allowlist", "player", player),
            notification("allowlist/removed", "Player was removed from allowlist", "player", player),
            notification("ip_bans/added", "Ip was added to ip ban list", "player", schema::ref("ip_ban")),
            notification("ip_bans/removed", "Ip was removed from ip ban list", "player", schema::type("string")),
            notification("bans/added", "Player was added to ban list", "player", schema::ref("user_ban")),
            notification("bans/removed", "Player was removed from ban list", "player", player),
            notification("gamerules/updated", "Gamerule was changed", "gamerule", schema::ref("typed_game_rule")),
            notification("server/status", "Server status heartbeat, including before the server has spun up", "status",
                         schema::ref("server_state")),
        };
    }();
    return methods;
}

const IncomingMethod* findIncoming(const std::string_view name) {
    for (const IncomingMethod& method : incomingMethods()) {
        if (method.name == name) {
            return &method;
        }
    }
    return nullptr;
}

} // namespace basalt::server::remote::managementMethods
