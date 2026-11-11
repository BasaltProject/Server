#include "ManagementSchema.hpp"

#include "ManagementMethods.hpp"

namespace basalt::server::remote::managementSchema {
namespace {

using wire::common::JsonObject;

constexpr std::string_view kOpenRpcVersion = "1.3.2";
constexpr std::string_view kTitle = "Minecraft Server JSON-RPC";
constexpr std::string_view kApiVersion = "3.1.0";
constexpr std::string_view kNamespace = "minecraft:";

[[nodiscard]] JsonValue text(std::string_view value) { return JsonValue::of(std::string{value}); }

[[nodiscard]] JsonValue components() {
    const JsonValue string = type("string");
    const JsonValue boolean = type("boolean");
    const JsonValue integer = type("integer");
    const JsonValue flexible = types({"boolean", "integer"});
    JsonObject schemas;
    schemas.set("difficulty", enumOf({"peaceful", "easy", "normal", "hard"}));
    schemas.set("game_type", enumOf({"survival", "creative", "adventure", "spectator"}));
    schemas.set("player", record({{"id", string}, {"name", string}}));
    schemas.set("version", record({{"name", string}, {"protocol", integer}}));
    schemas.set("server_state",
                record({{"started", boolean}, {"players", array(ref("player"))}, {"version", ref("version")}}));
    schemas.set("typed_game_rule",
                record({{"key", string}, {"value", flexible}, {"type", enumOf({"integer", "boolean"})}}));
    schemas.set("untyped_game_rule", record({{"key", string}, {"value", flexible}}));
    schemas.set("message",
                record({{"literal", string}, {"translatable", string}, {"translatableParams", array(string)}}));
    schemas.set(
        "system_message",
        record({{"message", ref("message")}, {"overlay", boolean}, {"receivingPlayers", array(ref("player"))}}));
    schemas.set("kick_player", record({{"message", ref("message")}, {"player", ref("player")}}));
    schemas.set("operator",
                record({{"player", ref("player")}, {"bypassesPlayerLimit", boolean}, {"permissionLevel", integer}}));
    schemas.set(
        "incoming_ip_ban",
        record(
            {{"player", ref("player")}, {"ip", string}, {"reason", string}, {"source", string}, {"expires", string}}));
    schemas.set("ip_ban", record({{"ip", string}, {"reason", string}, {"source", string}, {"expires", string}}));
    schemas.set("user_ban",
                record({{"player", ref("player")}, {"reason", string}, {"source", string}, {"expires", string}}));
    JsonObject wrapper;
    wrapper.set("schemas", JsonValue::ofObject(std::move(schemas)));
    return JsonValue::ofObject(std::move(wrapper));
}

[[nodiscard]] JsonValue namedMethod(std::string_view name, std::string_view description, std::string_view paramName,
                                    const JsonValue& paramSchema, const std::string* resultName,
                                    const JsonValue* resultSchema) {
    JsonObject object;
    object.set("name", text(std::string{kNamespace} + std::string{name}));
    object.set("description", text(description));
    std::vector<JsonValue> params;
    if (!paramName.empty()) {
        JsonObject param;
        param.set("name", text(paramName));
        param.set("schema", paramSchema);
        param.set("required", JsonValue::of(true));
        params.push_back(JsonValue::ofObject(std::move(param)));
    }
    object.set("params", JsonValue::ofArray(std::move(params)));
    if (resultName != nullptr) {
        JsonObject result;
        result.set("name", text(*resultName));
        result.set("schema", *resultSchema);
        object.set("result", JsonValue::ofObject(std::move(result)));
    }
    return JsonValue::ofObject(std::move(object));
}

} // namespace

JsonValue type(const std::string_view name) {
    JsonObject object;
    object.set("type", text(name));
    return JsonValue::ofObject(std::move(object));
}

JsonValue types(const std::initializer_list<std::string_view> names) {
    std::vector<JsonValue> list;
    for (const std::string_view name : names) {
        list.push_back(text(name));
    }
    JsonObject object;
    object.set("type", JsonValue::ofArray(std::move(list)));
    return JsonValue::ofObject(std::move(object));
}

JsonValue ref(const std::string_view component) {
    JsonObject object;
    object.set("$ref", text("#/components/schemas/" + std::string{component}));
    return JsonValue::ofObject(std::move(object));
}

JsonValue array(JsonValue items) {
    JsonObject object;
    object.set("type", text("array"));
    object.set("items", std::move(items));
    return JsonValue::ofObject(std::move(object));
}

JsonValue enumOf(const std::initializer_list<std::string_view> values) {
    std::vector<JsonValue> list;
    for (const std::string_view value : values) {
        list.push_back(text(value));
    }
    JsonObject object;
    object.set("type", text("string"));
    object.set("enum", JsonValue::ofArray(std::move(list)));
    return JsonValue::ofObject(std::move(object));
}

JsonValue record(const std::initializer_list<std::pair<std::string_view, JsonValue>> properties) {
    JsonObject fields;
    for (const auto& [name, schema] : properties) {
        fields.set(name, schema);
    }
    JsonObject object;
    object.set("type", text("object"));
    object.set("properties", JsonValue::ofObject(std::move(fields)));
    return JsonValue::ofObject(std::move(object));
}

JsonValue discoverDocument() {
    std::vector<JsonValue> methods;
    for (const managementMethods::IncomingMethod& method : managementMethods::incomingMethods()) {
        if (method.discoverable) {
            methods.push_back(namedMethod(method.name, method.description, method.paramName, method.paramSchema,
                                          &method.resultName, &method.resultSchema));
        }
    }
    for (const managementMethods::OutgoingMethod& method : managementMethods::outgoingMethods()) {
        methods.push_back(
            namedMethod(method.name, method.description, method.paramName, method.paramSchema, nullptr, nullptr));
    }
    JsonObject info;
    info.set("title", text(kTitle));
    info.set("version", text(kApiVersion));
    JsonObject document;
    document.set("openrpc", text(kOpenRpcVersion));
    document.set("info", JsonValue::ofObject(std::move(info)));
    document.set("methods", JsonValue::ofArray(std::move(methods)));
    document.set("components", components());
    return JsonValue::ofObject(std::move(document));
}

} // namespace basalt::server::remote::managementSchema
