#include "WasmWorlds.hpp"

#include <basalt/server/plugins/PluginPlayerOps.hpp>

#include <algorithm>
#include <array>

namespace basalt::server::plugins::wasm::worlds {
namespace {
constexpr std::int32_t kGlobal = 64;
constexpr std::int32_t kBreak = 128;
constexpr std::int32_t kFire = 256;

constexpr std::array<std::string_view, 11> kSoundCategories{
    "master", "music", "record", "weather", "block", "hostile", "neutral", "player", "ambient", "voice", "ui"};

/// The player effects this area owns; each must also be an operation or property of the player tables.
constexpr std::array<std::string_view, 19> kViewOperations{
    "sound_at",        "sound_entity",      "stop_sound",         "particle_at",  "world_event",
    "set_player_time", "reset_player_time", "set_player_weather", "block_change", "block_damage",
    "sign_text",       "open_sign",         "game_event",         "border_set",   "border_reset",
    "block_entity",    "post_effects",      "set_post_effects",   "entity_event"};

constexpr std::array<std::string_view, 5> kViewProperties{"player_time", "player_time_relative", "player_weather",
                                                          "view_distance", "simulation_distance"};

[[nodiscard]] bool isOperation(std::string_view name) {
    return std::ranges::contains(kViewOperations, name) &&
           std::ranges::any_of(kPlayerOperations, [&](const EntityOperationSpec& spec) { return spec.name == name; });
}

[[nodiscard]] bool isProperty(std::string_view name) {
    return std::ranges::contains(kViewProperties, name) &&
           std::ranges::any_of(kPlayerProperties, [&](const EntityPropertySpec& spec) { return spec.name == name; });
}

[[nodiscard]] std::optional<PluginEntityValue> toEntityValue(const Value& value) {
    if (value.isNull()) {
        return PluginEntityValue{};
    }
    if (const bool* flag = value.asBool()) {
        return PluginEntityValue{*flag};
    }
    if (const std::int64_t* whole = value.asInt()) {
        return PluginEntityValue{*whole};
    }
    if (const double* real = value.asFloat()) {
        return PluginEntityValue{*real};
    }
    if (const std::string* text = value.asString()) {
        return PluginEntityValue{*text};
    }
    if (const Value::List* list = value.asList()) {
        std::vector<std::string> texts;
        for (const Value& item : *list) {
            const std::string* text = item.asString();
            if (text == nullptr) {
                return std::nullopt;
            }
            texts.push_back(*text);
        }
        return PluginEntityValue{std::move(texts)};
    }
    return std::nullopt;
}

[[nodiscard]] Value fromEntityValue(const PluginEntityValue& value) {
    if (const bool* flag = std::get_if<bool>(&value)) {
        return Value{*flag};
    }
    if (const std::int64_t* whole = std::get_if<std::int64_t>(&value)) {
        return Value{*whole};
    }
    if (const double* real = std::get_if<double>(&value)) {
        return Value{*real};
    }
    if (const std::string* text = std::get_if<std::string>(&value)) {
        return Value{*text};
    }
    if (const std::vector<std::string>* texts = std::get_if<std::vector<std::string>>(&value)) {
        return textList(*texts);
    }
    return Value{};
}

void sound(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::optional<std::string_view> id = c.memory().string(c.i32(1), c.i32(2));
        const std::optional<std::string_view> category = c.memory().string(c.i32(3), c.i32(4));
        if (!id || id->empty() || !category) {
            return abi::kErrInvalid;
        }
        const std::string_view named = category->empty() ? "master" : *category;
        if (!std::ranges::contains(kSoundCategories, named)) {
            return abi::kErrInvalid;
        }
        PluginWorldEnv* env = world.env();
        if (env == nullptr) {
            return abi::kErrAbsent;
        }
        env->sound(*id, named, c.f64(5), c.f64(6), c.f64(7), c.f32(8), c.f32(9), c.i64(10));
        return abi::kOk;
    });
}

void particle(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::expected<Value, std::int32_t> spec = mapArgument(c.memory(), c.i32(1), c.i32(2));
        if (!spec) {
            return spec.error();
        }
        PluginWorldEnv* env = world.env();
        PluginParticle out;
        out.type = textField(*spec, "type");
        if (out.type.empty()) {
            return abi::kErrInvalid;
        }
        out.x = realField(*spec, "x");
        out.y = realField(*spec, "y");
        out.z = realField(*spec, "z");
        out.count = static_cast<std::int32_t>(intField(*spec, "count", 1));
        out.offsetX = static_cast<float>(realField(*spec, "offset_x"));
        out.offsetY = static_cast<float>(realField(*spec, "offset_y"));
        out.offsetZ = static_cast<float>(realField(*spec, "offset_z"));
        const auto speed = static_cast<float>(realField(*spec, "speed"));
        out.speed = {static_cast<float>(realField(*spec, "speed_x", static_cast<double>(speed))),
                     static_cast<float>(realField(*spec, "speed_y", static_cast<double>(speed))),
                     static_cast<float>(realField(*spec, "speed_z", static_cast<double>(speed)))};
        out.randomization = static_cast<std::int32_t>(intField(*spec, "randomization"));
        out.force = boolField(*spec, "force");
        out.item = textField(*spec, "item");
        out.receiver = textField(*spec, "player");
        if (const Value* data = spec->find("data"); data != nullptr && data->asList() != nullptr) {
            for (const Value& item : *data->asList()) {
                const double* real = item.asFloat();
                const std::int64_t* whole = item.asInt();
                out.data.push_back(real != nullptr ? *real : whole != nullptr ? static_cast<double>(*whole) : 0.0);
            }
        }
        if (env == nullptr) {
            return abi::kErrAbsent;
        }
        env->particle(out);
        return abi::kOk;
    });
}

void levelEvent(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        PluginWorldEnv* env = world.env();
        if (env == nullptr) {
            return abi::kErrAbsent;
        }
        env->levelEvent(c.i32(1), c.i32(2), c.i32(3), c.i32(4), c.i32(5), (c.i32(6) & kGlobal) != 0);
        return abi::kOk;
    });
}

void blockEvent(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        PluginWorldEnv* env = world.env();
        if (env == nullptr) {
            return abi::kErrAbsent;
        }
        env->blockEvent(c.i32(1), c.i32(2), c.i32(3), c.i32(4), c.i32(5));
        return abi::kOk;
    });
}

void explode(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::int32_t flags = c.i32(5);
        return world.explode(c.f64(1), c.f64(2), c.f64(3), c.f32(4), (flags & kBreak) != 0, c.i32(6),
                             (flags & kFire) != 0)
                   ? 1
                   : 0;
    });
}

void lightning(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::shared_ptr<PluginEntityRef> bolt =
            world.spawn("minecraft:lightning_bolt", c.f64(1), c.f64(2), c.f64(3));
        return bolt == nullptr ? abi::kErrAbsent : bolt->id();
    });
}

void gameEvent(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::optional<std::string_view> event = c.memory().string(c.i32(1), c.i32(2));
        const std::optional<std::string_view> source = c.memory().string(c.i32(6), c.i32(7));
        PluginWorldEnv* env = world.env();
        if (!event || event->empty() || !source) {
            return abi::kErrInvalid;
        }
        if (env == nullptr) {
            return abi::kErrAbsent;
        }
        env->gameEvent(*event, c.f64(3), c.f64(4), c.f64(5), *source);
        return abi::kOk;
    });
}

void playerEffect(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> op = c.memory().string(c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::expected<Value, std::int32_t> args = c.i32(4) == 0
                                                        ? std::expected<Value, std::int32_t>{Value{Value::List{}}}
                                                        : readValue(c.memory(), c.i32(3), c.i32(4));
    if (!op || !args || args->asList() == nullptr) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const bool operation = isOperation(*op);
    if (!operation && !isProperty(*op)) {
        c.ret(abi::kErrAbsent);
        return;
    }
    std::vector<PluginEntityValue> values;
    for (const Value& item : *args->asList()) {
        std::optional<PluginEntityValue> converted = toEntityValue(item);
        if (!converted) {
            c.ret(abi::kErrInvalid);
            return;
        }
        values.push_back(std::move(*converted));
    }
    std::optional<PluginEntityValue> answer;
    if (operation) {
        answer = player->invoke(*op, values);
    } else if (values.empty()) {
        answer = player->property(*op);
    } else if (values.size() == 1) {
        answer = player->setProperty(*op, values.front()) ? std::optional{PluginEntityValue{true}} : std::nullopt;
    } else {
        c.ret(abi::kErrInvalid);
        return;
    }
    if (!answer) {
        c.ret(abi::kErrAbsent);
        return;
    }
    c.ret(putValue(c.memory(), c.i32(5), c.i32(6), fromEntityValue(*answer)));
}

void borderGet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        PluginWorldEnv* env = world.env();
        if (env == nullptr) {
            return abi::kErrAbsent;
        }
        const PluginBorder border = env->border();
        Value::Map map;
        put(map, "center_x", Value{border.centerX});
        put(map, "center_z", Value{border.centerZ});
        put(map, "size", Value{border.size});
        put(map, "target_size", Value{border.targetSize});
        put(map, "lerp_ticks", Value{border.lerpTicks});
        put(map, "damage_amount", Value{border.damagePerBlock});
        put(map, "damage_buffer", Value{border.damageBuffer});
        put(map, "warning_distance", Value{static_cast<std::int64_t>(border.warningBlocks)});
        put(map, "warning_time", Value{static_cast<std::int64_t>(border.warningSeconds)});
        return putValue(c.memory(), c.i32(1), c.i32(2), Value{std::move(map)});
    });
}

void borderSet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::expected<Value, std::int32_t> spec = mapArgument(c.memory(), c.i32(1), c.i32(2));
        if (!spec) {
            return spec.error();
        }
        PluginWorldEnv* env = world.env();
        if (env == nullptr) {
            return abi::kErrAbsent;
        }
        PluginBorder border = env->border();
        border.centerX = realField(*spec, "center_x", border.centerX);
        border.centerZ = realField(*spec, "center_z", border.centerZ);
        border.size = realField(*spec, "size", border.size);
        border.damagePerBlock = realField(*spec, "damage_amount", border.damagePerBlock);
        border.damageBuffer = realField(*spec, "damage_buffer", border.damageBuffer);
        border.warningBlocks = static_cast<std::int32_t>(intField(*spec, "warning_distance", border.warningBlocks));
        border.warningSeconds = static_cast<std::int32_t>(intField(*spec, "warning_time", border.warningSeconds));
        const double seconds = realField(*spec, "seconds");
        if (!(border.size >= 0.0) || seconds < 0.0 || border.damageBuffer < 0.0) {
            return abi::kErrInvalid;
        }
        env->setBorder(border, seconds);
        return abi::kOk;
    });
}
} // namespace
} // namespace basalt::server::plugins::wasm::worlds

namespace basalt::server::plugins::wasm {
void addWorldsEffectsImports(State& state, std::vector<abi::HostImport>& out) {
    using namespace worlds;
    void* user = &state;
    out.insert(out.end(), {
                              {"world_sound", sound, user},
                              {"world_particle", particle, user},
                              {"world_level_event", levelEvent, user},
                              {"world_block_event", blockEvent, user},
                              {"world_explode", explode, user},
                              {"world_lightning", lightning, user},
                              {"world_game_event", gameEvent, user},
                              {"world_player_effect", playerEffect, user},
                              {"world_border_get", borderGet, user},
                              {"world_border_set", borderSet, user},
                          });
}
} // namespace basalt::server::plugins::wasm
