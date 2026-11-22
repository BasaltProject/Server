#include "WasmEventValues.hpp"

#include <basalt/server/plugins/PluginPlayerOps.hpp>

#include <cmath>

namespace basalt::server::plugins::wasm {
namespace {
[[nodiscard]] const char* kindName(EntityValueKind kind) {
    switch (kind) {
    case EntityValueKind::Bool:
        return "bool";
    case EntityValueKind::Int:
        return "int";
    case EntityValueKind::Real:
        return "real";
    case EntityValueKind::Text:
        return "text";
    case EntityValueKind::List:
        return "list";
    }
    return "text";
}

void playerByUuid(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> uuid = c.memory().string(c.i32(0), c.i32(1));
    const std::shared_ptr<PluginPlayerRef> player = uuid ? state.host->services().players().byUuid(*uuid) : nullptr;
    c.ret(player == nullptr || !player->online() ? 0 : state.playerHandle(player));
}

void playerUuid(void* user, abi::Call& c) {
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = stateOf(user).resolvePlayer(c.i32(0), player);
    c.ret(status != abi::kOk ? status : c.memory().put(c.i32(1), c.i32(2), player->uuid()));
}

[[nodiscard]] std::optional<Value> builtin(const PluginPlayerRef& player, std::string_view name) {
    if (name == "name") {
        return Value{player.name()};
    }
    if (name == "uuid") {
        return Value{player.uuid()};
    }
    if (name == "online") {
        return Value{player.online()};
    }
    if (name == "health") {
        return Value{player.health()};
    }
    if (name == "food") {
        return Value{static_cast<std::int64_t>(player.food())};
    }
    if (name == "gamemode") {
        return Value{player.gamemode()};
    }
    if (name == "level") {
        return Value{static_cast<std::int64_t>(player.level())};
    }
    if (name == "world") {
        return Value{player.world()};
    }
    if (name == "x") {
        return Value{player.x()};
    }
    if (name == "y") {
        return Value{player.y()};
    }
    if (name == "z") {
        return Value{player.z()};
    }
    if (name == "yaw") {
        return Value{static_cast<double>(player.yaw())};
    }
    if (name == "pitch") {
        return Value{static_cast<double>(player.pitch())};
    }
    if (name == "ping") {
        return Value{static_cast<std::int64_t>(player.ping())};
    }
    if (name == "sneaking") {
        return Value{player.sneaking()};
    }
    if (name == "flying") {
        return Value{player.flying()};
    }
    if (name == "on_ground") {
        return Value{player.onGround()};
    }
    if (name == "held_item") {
        const std::optional<std::string> held = player.heldItem();
        return held ? Value{*held} : Value{};
    }
    if (name == "address") {
        return Value{player.address()};
    }
    if (name == "permission_level") {
        return Value{static_cast<std::int64_t>(player.permissionLevel())};
    }
    return std::nullopt;
}

void playerGet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status != abi::kOk || !name) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    std::optional<Value> value = builtin(*player, *name);
    if (!value && playerPropertyNamed(*name) != nullptr) {
        value = valueOf(player->property(*name).value_or(PluginEntityValue{}));
    }
    c.ret(value ? putValue(memory, c.i32(3), c.i32(4), *value) : abi::kErrAbsent);
}

[[nodiscard]] std::optional<double> numberOf(const Value& value) {
    if (const double* real = value.asFloat()) {
        return *real;
    }
    if (const std::int64_t* whole = value.asInt()) {
        return static_cast<double>(*whole);
    }
    return std::nullopt;
}

/// The value as the kind the property declares, or nothing when it does not fit.
[[nodiscard]] std::optional<PluginEntityValue> fittedProperty(const EntityPropertySpec& spec, const Value& value) {
    switch (spec.kind) {
    case EntityValueKind::Bool:
        return value.asBool() != nullptr ? entityValueOf(value) : std::nullopt;
    case EntityValueKind::Int:
        if (const double* real = value.asFloat(); real != nullptr && std::trunc(*real) == *real) {
            return PluginEntityValue{static_cast<std::int64_t>(*real)};
        }
        return value.asInt() != nullptr ? entityValueOf(value) : std::nullopt;
    case EntityValueKind::Real:
        if (const std::optional<double> real = numberOf(value)) {
            return PluginEntityValue{*real};
        }
        return std::nullopt;
    case EntityValueKind::Text:
        return value.asString() != nullptr ? entityValueOf(value) : std::nullopt;
    case EntityValueKind::List:
        return value.asList() != nullptr ? entityValueOf(value) : std::nullopt;
    }
    return std::nullopt;
}

[[nodiscard]] std::int32_t writeBuiltin(PluginPlayerRef& player, std::string_view name, const Value& value,
                                        bool& handled) {
    handled = true;
    if (name == "health") {
        const std::optional<double> health = numberOf(value);
        if (!health || !std::isfinite(*health) || *health < 0.0) {
            return abi::kErrInvalid;
        }
        player.setHealth(*health);
        return abi::kOk;
    }
    if (name == "food" || name == "level") {
        const std::int64_t* whole = value.asInt();
        if (whole == nullptr || *whole < 0 || *whole > 1000000) {
            return abi::kErrInvalid;
        }
        if (name == "food") {
            player.setFood(static_cast<std::int32_t>(*whole));
        } else {
            player.setLevel(static_cast<std::int32_t>(*whole));
        }
        return abi::kOk;
    }
    if (name == "gamemode") {
        const std::string* mode = value.asString();
        if (mode == nullptr) {
            return abi::kErrInvalid;
        }
        player.setGamemode(*mode);
        return abi::kOk;
    }
    handled = false;
    return abi::kOk;
}

void playerSet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(1), c.i32(2));
    const auto value = readValue(memory, c.i32(3), c.i32(4));
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status != abi::kOk || !name || !value) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    bool handled = false;
    const std::int32_t result = writeBuiltin(*player, *name, *value, handled);
    if (handled) {
        c.ret(result);
        return;
    }
    const EntityPropertySpec* spec = playerPropertyNamed(*name);
    const std::optional<PluginEntityValue> fitted = spec != nullptr ? fittedProperty(*spec, *value) : std::nullopt;
    c.ret(spec != nullptr && spec->writable && fitted && player->setProperty(*name, *fitted) ? abi::kOk
                                                                                             : abi::kErrInvalid);
}

void playerCall(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> operation = memory.string(c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    std::vector<PluginEntityValue> arguments;
    if (status == abi::kOk && !operation) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk && c.i32(4) != 0) {
        const auto list = readValue(memory, c.i32(3), c.i32(4));
        const Value::List* items = list ? list->asList() : nullptr;
        status = items == nullptr ? abi::kErrInvalid : abi::kOk;
        for (std::size_t at = 0; status == abi::kOk && at < items->size(); ++at) {
            std::optional<PluginEntityValue> argument = entityValueOf((*items)[at]);
            if (argument) {
                arguments.push_back(std::move(*argument));
            } else {
                status = abi::kErrInvalid;
            }
        }
    }
    if (status == abi::kOk && !isPlayerOperation(*operation)) {
        status = abi::kErrAbsent;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::optional<PluginEntityValue> result = player->invoke(*operation, arguments);
    c.ret(putValue(memory, c.i32(5), c.i32(6), result ? valueOf(*result) : Value{}));
}

void playerDescribe(void*, abi::Call& c) {
    Value::List properties;
    for (const EntityPropertySpec& spec : kPlayerProperties) {
        properties.emplace_back(Value::Map{{"name", Value{std::string{spec.name}}},
                                           {"kind", Value{kindName(spec.kind)}},
                                           {"writable", Value{spec.writable}}});
    }
    Value::List operations;
    for (const EntityOperationSpec& spec : kPlayerOperations) {
        operations.emplace_back(std::string{spec.name});
    }
    c.ret(putValue(
        c.memory(), c.i32(0), c.i32(1),
        Value{Value::Map{{"properties", Value{std::move(properties)}}, {"operations", Value{std::move(operations)}}}}));
}

void playerHealth(void* user, abi::Call& c) {
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = stateOf(user).resolvePlayer(c.i32(0), player);
    c.ret(status != abi::kOk ? status : (c.memory().write(c.i32(1), player->health()) ? abi::kOk : abi::kErrInvalid));
}

void playerSetHealth(void* user, abi::Call& c) {
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = stateOf(user).resolvePlayer(c.i32(0), player);
    const double health = c.f64(1);
    if (status == abi::kOk && std::isfinite(health) && health >= 0.0) {
        player->setHealth(health);
    }
    c.ret(status != abi::kOk ? status : (std::isfinite(health) && health >= 0.0 ? abi::kOk : abi::kErrInvalid));
}

void playerGamemode(void* user, abi::Call& c) {
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = stateOf(user).resolvePlayer(c.i32(0), player);
    c.ret(status != abi::kOk ? status : c.memory().put(c.i32(1), c.i32(2), player->gamemode()));
}

void playerSetGamemode(void* user, abi::Call& c) {
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = stateOf(user).resolvePlayer(c.i32(0), player);
    const std::optional<std::string_view> mode = c.memory().string(c.i32(1), c.i32(2));
    const bool known =
        mode && (*mode == "survival" || *mode == "creative" || *mode == "adventure" || *mode == "spectator");
    if (status == abi::kOk && known) {
        player->setGamemode(*mode);
    }
    c.ret(status != abi::kOk ? status : (known ? abi::kOk : abi::kErrInvalid));
}

void playerTeleport(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> world = c.memory().string(c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status == abi::kOk && (!world || !std::isfinite(c.f64(3)) || !std::isfinite(c.f64(4)) ||
                               !std::isfinite(c.f64(5)) || !std::isfinite(c.f32(6)) || !std::isfinite(c.f32(7)))) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        const std::string target = world->empty() ? player->world() : std::string{*world};
        status =
            player->teleport(target, c.f64(3), c.f64(4), c.f64(5), c.f32(6), c.f32(7)) ? abi::kOk : abi::kErrAbsent;
    }
    c.ret(status);
}

void playerTitle(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> title = memory.string(c.i32(1), c.i32(2));
    const std::optional<std::string_view> subtitle = memory.string(c.i32(3), c.i32(4));
    const std::optional<ArgumentList> arguments = readArguments(memory, c.i32(8), c.i32(9));
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status == abi::kOk && (!title || !subtitle || !arguments || c.i32(5) < 0 || c.i32(6) < 0 || c.i32(7) < 0)) {
        status = abi::kErrInvalid;
    }
    std::string main;
    std::string sub;
    if (status == abi::kOk &&
        (!renderText(state.plugin->text, *title, arguments->view(), main) ||
         (!subtitle->empty() && !renderText(state.plugin->text, *subtitle, arguments->view(), sub)))) {
        status = abi::kErrAbsent;
    }
    if (status == abi::kOk) {
        player->title(main, sub, c.i32(5), c.i32(6), c.i32(7));
    }
    c.ret(status);
}

void playerKick(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> text = memory.string(c.i32(1), c.i32(2));
    const std::optional<ArgumentList> arguments = readArguments(memory, c.i32(3), c.i32(4));
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status == abi::kOk && (!text || !arguments)) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        if (renderText(state.plugin->text, *text, arguments->view(), state.scratch)) {
            player->kick(state.scratch);
        } else {
            status = abi::kErrAbsent;
        }
    }
    c.ret(status);
}

void playerEffect(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> effect = c.memory().string(c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status == abi::kOk && (!effect || effect->empty() || c.i32(3) < -1 || c.i32(4) < 0)) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk && !player->effect(*effect, c.i32(3), c.i32(4))) {
        status = abi::kErrAbsent;
    }
    c.ret(status);
}
} // namespace

void addEventsPlayerImports(State& state, std::vector<abi::HostImport>& out) {
    State* user = &state;
    out.insert(out.end(), {
                              {"player_by_uuid", playerByUuid, user},
                              {"player_uuid", playerUuid, user},
                              {"player_get", playerGet, user},
                              {"player_set", playerSet, user},
                              {"player_call", playerCall, user},
                              {"player_describe", playerDescribe, user},
                              {"player_health", playerHealth, user},
                              {"player_set_health", playerSetHealth, user},
                              {"player_gamemode", playerGamemode, user},
                              {"player_set_gamemode", playerSetGamemode, user},
                              {"player_teleport", playerTeleport, user},
                              {"player_title", playerTitle, user},
                              {"player_kick", playerKick, user},
                              {"player_effect", playerEffect, user},
                          });
}
} // namespace basalt::server::plugins::wasm
