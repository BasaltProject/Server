#include "WasmWorlds.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace basalt::server::plugins::wasm::worlds {
namespace {
constexpr std::array<std::string_view, 8> kCategories{
    "monster",        "creature",      "ambient", "axolotls", "underground_water_creature",
    "water_creature", "water_ambient", "misc"};
constexpr std::string_view kTunablePrefix = "tunable.";

struct Property {
    std::string_view name;
    std::string_view type;
    bool writable;
    Value (*get)(PluginWorldRef&);
    std::int32_t (*set)(PluginWorldRef&, const Value&);
};

[[nodiscard]] Value whole(std::int64_t value) { return Value{value}; }

[[nodiscard]] std::int32_t unsupported(PluginWorldRef&, const Value&) { return abi::kErrAbsent; }

[[nodiscard]] std::optional<std::int64_t> asWhole(const Value& value) {
    if (const std::int64_t* number = value.asInt()) {
        return *number;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<double> asReal(const Value& value) {
    if (const double* real = value.asFloat()) {
        return *real;
    }
    if (const std::int64_t* number = value.asInt()) {
        return static_cast<double>(*number);
    }
    return std::nullopt;
}

[[nodiscard]] PluginWorldEnv* envOf(PluginWorldRef& world) { return world.env(); }

[[nodiscard]] Value spawnRules(PluginWorldRef& world) {
    PluginWorldEnv* env = envOf(world);
    const PluginSpawnTunables rules = env == nullptr ? PluginSpawnTunables{} : env->spawnTunables();
    Value::Map limits;
    Value::Map ticks;
    for (std::size_t at = 0; at < kCategories.size(); ++at) {
        put(limits, std::string{kCategories[at]}, whole(rules.limits[at]));
        put(ticks, std::string{kCategories[at]}, whole(rules.ticksPer[at]));
    }
    Value::Map map;
    put(map, "limits", Value{std::move(limits)});
    put(map, "ticks_per", Value{std::move(ticks)});
    put(map, "allow_monsters", Value{rules.allowMonsters});
    put(map, "allow_animals", Value{rules.allowAnimals});
    return Value{std::move(map)};
}

[[nodiscard]] std::int32_t setSpawnRules(PluginWorldRef& world, const Value& value) {
    PluginWorldEnv* env = envOf(world);
    if (env == nullptr) {
        return abi::kErrAbsent;
    }
    if (value.asMap() == nullptr) {
        return abi::kErrInvalid;
    }
    PluginSpawnTunables rules = env->spawnTunables();
    for (const auto& [key, target] : {std::pair{"limits", &rules.limits}, std::pair{"ticks_per", &rules.ticksPer}}) {
        const Value* group = value.find(key);
        if (group == nullptr) {
            continue;
        }
        if (group->asMap() == nullptr) {
            return abi::kErrInvalid;
        }
        for (std::size_t at = 0; at < kCategories.size(); ++at) {
            if (hasField(*group, kCategories[at])) {
                (*target)[at] = static_cast<std::int32_t>(intField(*group, kCategories[at]));
            }
        }
    }
    rules.allowMonsters = boolField(value, "allow_monsters", rules.allowMonsters);
    rules.allowAnimals = boolField(value, "allow_animals", rules.allowAnimals);
    env->setSpawnTunables(rules);
    return abi::kOk;
}

[[nodiscard]] Value clockValue(PluginWorldRef& world) {
    PluginWorldEnv* env = envOf(world);
    const PluginClock clock = env == nullptr ? PluginClock{} : env->clock();
    Value::List markers;
    for (const auto& [name, ticks] : clock.markers) {
        Value::Map marker;
        put(marker, "name", Value{name});
        put(marker, "ticks", whole(ticks));
        markers.emplace_back(std::move(marker));
    }
    Value::Map map;
    put(map, "total_ticks", whole(clock.totalTicks));
    put(map, "rate", Value{static_cast<double>(clock.rate)});
    put(map, "paused", Value{clock.paused});
    put(map, "markers", Value{std::move(markers)});
    return Value{std::move(map)};
}

[[nodiscard]] Value weatherTimes(PluginWorldRef& world) {
    PluginWorldEnv* env = envOf(world);
    const std::array<std::int32_t, 3> times = env == nullptr ? std::array<std::int32_t, 3>{} : env->weatherTimes();
    Value::Map map;
    put(map, "rain", whole(times[0]));
    put(map, "thunder", whole(times[1]));
    put(map, "clear", whole(times[2]));
    return Value{std::move(map)};
}

[[nodiscard]] std::int32_t setWeatherTimes(PluginWorldRef& world, const Value& value) {
    PluginWorldEnv* env = envOf(world);
    if (env == nullptr) {
        return abi::kErrAbsent;
    }
    if (value.asMap() == nullptr) {
        return abi::kErrInvalid;
    }
    std::array<std::int32_t, 3> times = env->weatherTimes();
    times[0] = static_cast<std::int32_t>(intField(value, "rain", times[0]));
    times[1] = static_cast<std::int32_t>(intField(value, "thunder", times[1]));
    times[2] = static_cast<std::int32_t>(intField(value, "clear", times[2]));
    env->setWeatherTimes(times[0], times[1], times[2]);
    return abi::kOk;
}

[[nodiscard]] Value spawnPoint(PluginWorldRef& world) {
    const std::array<std::int32_t, 3> at = world.spawn();
    return intList(at);
}

[[nodiscard]] std::int32_t setSpawnPoint(PluginWorldRef& world, const Value& value) {
    const Value::List* list = value.asList();
    if (list == nullptr || list->size() != 3) {
        return abi::kErrInvalid;
    }
    std::array<std::int32_t, 3> at{};
    for (std::size_t index = 0; index < 3; ++index) {
        const std::optional<std::int64_t> number = asWhole((*list)[index]);
        if (!number) {
            return abi::kErrInvalid;
        }
        at[index] = static_cast<std::int32_t>(*number);
    }
    world.setSpawn(at[0], at[1], at[2]);
    return abi::kOk;
}

[[nodiscard]] std::int32_t setBool(const Value& value, bool& out) {
    const bool* flag = value.asBool();
    if (flag == nullptr) {
        return abi::kErrInvalid;
    }
    out = *flag;
    return abi::kOk;
}

[[nodiscard]] std::int32_t setDifficulty(PluginWorldRef& world, const Value& value) {
    const std::string* text = value.asString();
    return text == nullptr || !world.setDifficulty(*text) ? abi::kErrInvalid : abi::kOk;
}

[[nodiscard]] std::int32_t setLocked(PluginWorldRef& world, const Value& value) {
    bool locked = false;
    const std::int32_t status = setBool(value, locked);
    if (status == abi::kOk) {
        world.setDifficultyLocked(locked);
    }
    return status;
}

[[nodiscard]] std::int32_t setDayTime(PluginWorldRef& world, const Value& value) {
    const std::optional<std::int64_t> ticks = asWhole(value);
    if (!ticks) {
        return abi::kErrInvalid;
    }
    world.setDayTime(*ticks);
    return abi::kOk;
}

[[nodiscard]] std::int32_t setViewDistance(PluginWorldRef& world, const Value& value) {
    const std::optional<std::int64_t> chunks = asWhole(value);
    PluginWorldEnv* env = envOf(world);
    if (!chunks || *chunks < 2 || *chunks > 32) {
        return abi::kErrInvalid;
    }
    if (env == nullptr) {
        return abi::kErrAbsent;
    }
    env->setViewDistance(static_cast<std::int32_t>(*chunks));
    return abi::kOk;
}

[[nodiscard]] std::int32_t setPluginData(PluginWorldRef& world, const Value& value) {
    const wire::wasm::Blob* blob = value.asBytes();
    if (blob == nullptr) {
        return abi::kErrInvalid;
    }
    return world.setPluginData(blob->data) ? abi::kOk : abi::kErrInvalid;
}

template <typename Fn>
[[nodiscard]] Value fromEnv(PluginWorldRef& world, Fn&& fn) {
    PluginWorldEnv* env = envOf(world);
    return env == nullptr ? Value{} : fn(*env);
}

const std::array kProperties{
    Property{"name", "string", false, [](PluginWorldRef& w) { return Value{w.name()}; }, unsupported},
    Property{"dimension", "string", false, [](PluginWorldRef& w) { return Value{w.dimension()}; }, unsupported},
    Property{"seed", "int", false, [](PluginWorldRef& w) { return whole(w.seed()); }, unsupported},
    Property{"min_height", "int", false, [](PluginWorldRef& w) { return whole(w.minHeight()); }, unsupported},
    Property{"height", "int", false, [](PluginWorldRef& w) { return whole(w.height()); }, unsupported},
    Property{"sea_level", "int", false,
             [](PluginWorldRef& w) { return fromEnv(w, [](PluginWorldEnv& e) { return whole(e.seaLevel()); }); },
             unsupported},
    Property{"view_distance", "int", true,
             [](PluginWorldRef& w) { return fromEnv(w, [](PluginWorldEnv& e) { return whole(e.viewDistance()); }); },
             setViewDistance},
    Property{"time", "int", false, [](PluginWorldRef& w) { return whole(w.time()); }, unsupported},
    Property{"day_time", "int", true, [](PluginWorldRef& w) { return whole(w.dayTime()); }, setDayTime},
    Property{"raining", "bool", false, [](PluginWorldRef& w) { return Value{w.raining()}; }, unsupported},
    Property{"thundering", "bool", false, [](PluginWorldRef& w) { return Value{w.thundering()}; }, unsupported},
    Property{"difficulty", "string", true, [](PluginWorldRef& w) { return Value{w.difficulty()}; }, setDifficulty},
    Property{"difficulty_locked", "bool", true, [](PluginWorldRef& w) { return Value{w.difficultyLocked()}; },
             setLocked},
    Property{"spawn", "list", true, spawnPoint, setSpawnPoint},
    Property{"folder", "string", false, [](PluginWorldRef& w) { return Value{w.folder()}; }, unsupported},
    Property{"player_count", "int", false,
             [](PluginWorldRef& w) { return fromEnv(w, [](PluginWorldEnv& e) { return whole(e.playerCount()); }); },
             unsupported},
    Property{
        "block_entity_count", "int", false,
        [](PluginWorldRef& w) { return fromEnv(w, [](PluginWorldEnv& e) { return whole(e.blockEntityCount()); }); },
        unsupported},
    Property{"tickable_block_entity_count", "int", false,
             [](PluginWorldRef& w) {
                 return fromEnv(w, [](PluginWorldEnv& e) { return whole(e.tickableBlockEntityCount()); });
             },
             unsupported},
    Property{"block_version", "int", false,
             [](PluginWorldRef& w) { return whole(static_cast<std::int64_t>(w.blockVersion())); }, unsupported},
    Property{"forced_chunks", "list", false,
             [](PluginWorldRef& w) {
                 return fromEnv(w, [](PluginWorldEnv& e) {
                     const std::vector<std::int32_t> chunks = e.forcedChunks();
                     return intList(chunks);
                 });
             },
             unsupported},
    Property{"weather_times", "map", true, weatherTimes, setWeatherTimes},
    Property{"clock", "map", false, clockValue, unsupported},
    Property{"spawn_rules", "map", true, spawnRules, setSpawnRules},
    Property{"rules", "list", false, [](PluginWorldRef& w) { return textList(w.gameRules()); }, unsupported},
    Property{"plugin_data", "bytes", true, [](PluginWorldRef& w) { return bytesValue(w.pluginData()); }, setPluginData},
};

[[nodiscard]] const Property* propertyNamed(std::string_view name) {
    const auto found = std::ranges::find(kProperties, name, &Property::name);
    return found == kProperties.end() ? nullptr : &*found;
}

[[nodiscard]] Value propertyTable() {
    Value::List list;
    for (const Property& property : kProperties) {
        Value::Map row;
        put(row, "name", Value{std::string{property.name}});
        put(row, "type", Value{std::string{property.type}});
        put(row, "writable", Value{property.writable});
        list.emplace_back(std::move(row));
    }
    return Value{std::move(list)};
}

[[nodiscard]] std::int32_t readProperty(PluginWorldRef& world, std::string_view key, Value& out) {
    if (key == "properties") {
        out = propertyTable();
        return abi::kOk;
    }
    if (key.starts_with(kTunablePrefix)) {
        PluginWorldEnv* env = envOf(world);
        const std::optional<double> number = env == nullptr ? std::nullopt : env->tunable(key.substr(8));
        if (!number) {
            return abi::kErrAbsent;
        }
        out = Value{*number};
        return abi::kOk;
    }
    const Property* property = propertyNamed(key);
    if (property == nullptr) {
        return abi::kErrAbsent;
    }
    out = property->get(world);
    return abi::kOk;
}

[[nodiscard]] std::int32_t writeProperty(PluginWorldRef& world, std::string_view key, const Value& value) {
    if (key == "weather") {
        if (value.asMap() == nullptr) {
            return abi::kErrInvalid;
        }
        world.setWeather(boolField(value, "raining"), boolField(value, "thundering"));
        return abi::kOk;
    }
    if (key == "clock_rate") {
        const std::optional<double> rate = asReal(value);
        PluginWorldEnv* env = envOf(world);
        if (!rate) {
            return abi::kErrInvalid;
        }
        return env != nullptr && env->setClockRate(static_cast<float>(*rate)) ? abi::kOk : abi::kErrInvalid;
    }
    if (key.starts_with(kTunablePrefix)) {
        const std::optional<double> number = asReal(value);
        PluginWorldEnv* env = envOf(world);
        if (!number) {
            return abi::kErrInvalid;
        }
        return env != nullptr && env->setTunable(key.substr(8), *number) ? abi::kOk : abi::kErrAbsent;
    }
    const Property* property = propertyNamed(key);
    return property == nullptr || !property->writable ? abi::kErrAbsent : property->set(world, value);
}

void worldByName(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> name = c.memory().string(c.i32(0), c.i32(1));
    const std::shared_ptr<PluginWorldRef> found =
        name ? state.host->services().worlds().byName(*name) : std::shared_ptr<PluginWorldRef>{};
    c.ret(found == nullptr ? 0 : handleOf(state, found));
}

void worldDefault(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::shared_ptr<PluginWorldRef> found = state.host->services().worlds().defaultWorld();
    c.ret(found == nullptr ? 0 : handleOf(state, found));
}

void worldList(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::vector<std::int32_t> handles;
    for (const std::shared_ptr<PluginWorldRef>& world : state.host->services().worlds().all()) {
        if (const std::int32_t handle = handleOf(state, world); handle > 0) {
            handles.push_back(handle);
        }
    }
    c.ret(putInts(c.memory(), c.i32(0), c.i32(1), handles));
}

void worldName(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) { return c.memory().put(c.i32(1), c.i32(2), world.name()); });
}

void worldGet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::optional<std::string_view> key = c.memory().string(c.i32(1), c.i32(2));
        Value value;
        if (!key) {
            return abi::kErrInvalid;
        }
        const std::int32_t status = readProperty(world, *key, value);
        return status != abi::kOk ? status : putValue(c.memory(), c.i32(3), c.i32(4), value);
    });
}

void worldSet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::optional<std::string_view> key = c.memory().string(c.i32(1), c.i32(2));
        const std::expected<Value, std::int32_t> value = readValue(c.memory(), c.i32(3), c.i32(4));
        if (!key || !value) {
            return abi::kErrInvalid;
        }
        return writeProperty(world, *key, *value);
    });
}

void worldRuleGet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::optional<std::string_view> name = c.memory().string(c.i32(1), c.i32(2));
        if (!name) {
            return abi::kErrInvalid;
        }
        const std::optional<std::string> rule = world.gameRule(*name);
        return rule ? c.memory().put(c.i32(3), c.i32(4), *rule) : abi::kErrAbsent;
    });
}

void worldRuleSet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::optional<std::string_view> name = c.memory().string(c.i32(1), c.i32(2));
        const std::optional<std::string_view> value = c.memory().string(c.i32(3), c.i32(4));
        if (!name || !value) {
            return abi::kErrInvalid;
        }
        if (!world.gameRule(*name)) {
            return abi::kErrAbsent;
        }
        return world.setGameRule(*name, *value) ? abi::kOk : abi::kErrInvalid;
    });
}

void worldCreate(void* user, abi::Call& c) {
    State& state = stateOf(user);
    if (!state.capable("worldgen")) {
        c.ret(abi::kErrDenied);
        return;
    }
    const std::expected<Value, std::int32_t> spec = mapArgument(c.memory(), c.i32(0), c.i32(1));
    if (!spec) {
        c.ret(spec.error());
        return;
    }
    const std::string name = textField(*spec, "name");
    PluginWorlds& worlds = state.host->services().worlds();
    if (name.empty() || worlds.byName(name) != nullptr) {
        c.ret(abi::kErrInvalid);
        return;
    }
    std::optional<std::int64_t> seed;
    if (hasField(*spec, "seed")) {
        seed = intField(*spec, "seed");
    }
    state.host->pauseWatchdog();
    const std::shared_ptr<PluginWorldRef> made =
        worlds.create(name, textField(*spec, "dimension", "overworld"), textField(*spec, "type", "normal"), seed);
    state.host->resumeWatchdog(state.plugin->manifest.id);
    c.ret(made == nullptr ? abi::kErrAbsent : handleOf(state, made));
}

void worldUnload(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> name = c.memory().string(c.i32(0), c.i32(1));
    if (!state.capable("worldgen")) {
        c.ret(abi::kErrDenied);
        return;
    }
    PluginWorlds& worlds = state.host->services().worlds();
    if (!name) {
        c.ret(abi::kErrInvalid);
    } else if (worlds.byName(*name) == nullptr) {
        c.ret(abi::kErrAbsent);
    } else {
        state.host->pauseWatchdog();
        const bool done = worlds.unload(*name, (c.i32(2) & 16) != 0);
        state.host->resumeWatchdog(state.plugin->manifest.id);
        c.ret(done ? abi::kOk : abi::kErrInvalid);
    }
}

void worldDelete(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> name = c.memory().string(c.i32(0), c.i32(1));
    if (!state.capable("worldgen")) {
        c.ret(abi::kErrDenied);
    } else if (!name) {
        c.ret(abi::kErrInvalid);
    } else {
        state.host->pauseWatchdog();
        const bool erased = state.host->services().worlds().erase(*name);
        state.host->resumeWatchdog(state.plugin->manifest.id);
        c.ret(erased ? abi::kOk : abi::kErrAbsent);
    }
}

void worldAt(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginWorldRef> world;
    const std::int32_t status = resolve(state, c.i32(0), world);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::int32_t request = beginRequest(state, c.i32(3));
    if (request == 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::shared_ptr<State*> link = state.link;
    const bool queued = state.host->services().at(world->name(), c.f64(1), c.f64(2), [link, request] {
        State* alive = *link;
        if (alive == nullptr) {
            return;
        }
        static_cast<void>(alive->host->services().tasks().after(0, [link, request] {
            if (State* later = *link; later != nullptr) {
                completeRequest(*later, request, Value{});
            }
        }));
    });
    if (!queued) {
        static_cast<void>(requests(state).remove(request));
        c.ret(abi::kErrAbsent);
        return;
    }
    c.ret(request);
}
} // namespace

std::int32_t handleOf(State& state, const std::shared_ptr<PluginWorldRef>& world) {
    Registry& all = registry(state);
    const std::string name = world->name();
    if (const auto known = all.byName.find(name); known != all.byName.end()) {
        std::shared_ptr<PluginWorldRef>* held = nullptr;
        if (all.handles.find(known->second, held) == abi::kOk && (*held)->identity() == world->identity()) {
            return known->second;
        }
        static_cast<void>(all.handles.remove(known->second));
        all.byName.erase(known);
    }
    const std::int32_t handle = all.handles.add(world);
    if (handle > 0) {
        all.byName[name] = handle;
    }
    return handle;
}

std::int32_t resolve(State& state, std::int32_t handle, std::shared_ptr<PluginWorldRef>& out) {
    Registry& all = registry(state);
    std::shared_ptr<PluginWorldRef>* held = nullptr;
    const std::int32_t status = all.handles.find(handle, held);
    if (status != abi::kOk) {
        return status;
    }
    const std::string name = (*held)->name();
    const std::shared_ptr<PluginWorldRef> live = state.host->services().worlds().byName(name);
    if (live == nullptr || live->identity() != (*held)->identity()) {
        all.byName.erase(name);
        static_cast<void>(all.handles.remove(handle));
        return abi::kErrStale;
    }
    out = *held;
    return abi::kOk;
}
} // namespace basalt::server::plugins::wasm::worlds

namespace basalt::server::plugins::wasm {
void addWorldsImports(State& state, std::vector<abi::HostImport>& out) {
    using namespace worlds;
    void* user = &state;
    out.insert(out.end(), {
                              {"world_by_name", worldByName, user},
                              {"world_default", worldDefault, user},
                              {"world_list", worldList, user},
                              {"world_name", worldName, user},
                              {"world_get", worldGet, user},
                              {"world_set", worldSet, user},
                              {"world_rule_get", worldRuleGet, user},
                              {"world_rule_set", worldRuleSet, user},
                              {"world_create", worldCreate, user},
                              {"world_unload", worldUnload, user},
                              {"world_delete", worldDelete, user},
                              {"world_at", worldAt, user},
                          });
}
} // namespace basalt::server::plugins::wasm
