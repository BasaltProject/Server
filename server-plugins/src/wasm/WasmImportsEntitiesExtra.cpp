#include "WasmEntities.hpp"

#include <basalt/server/plugins/PluginPlayerOps.hpp>
#include <basalt/server/plugins/PluginWorldEnv.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <algorithm>
#include <set>

namespace basalt::server::plugins::wasm {
namespace {
using entities::Target;
using wire::wasm::Value;

constexpr std::array<std::string_view, 8> kCategories{
    "monster",        "creature",      "ambient", "axolotls", "underground_water_creature",
    "water_creature", "water_ambient", "misc"};

constexpr std::array<std::string_view, 17> kBaseNames{"id",  "type",  "world",  "uuid",  "alive",   "x",
                                                      "y",   "z",     "yaw",    "pitch", "health",  "on_ground",
                                                      "age", "width", "height", "pos",   "velocity"};

[[nodiscard]] bool baseWritable(std::string_view name) {
    return name == "yaw" || name == "pitch" || name == "velocity";
}

[[nodiscard]] std::array<float, 2> facing(const Target& target) {
    return target.entity != nullptr ? target.entity->rotation()
                                    : std::array<float, 2>{target.player->yaw(), target.player->pitch()};
}

[[nodiscard]] std::optional<Value> baseValue(const Target& target, std::string_view name) {
    if (name == "id") {
        return Value{static_cast<std::int64_t>(target.id())};
    }
    if (name == "type") {
        return Value{target.type()};
    }
    if (name == "world") {
        return Value{target.world()};
    }
    if (name == "uuid") {
        return Value{target.uuid()};
    }
    if (name == "alive") {
        return Value{target.alive()};
    }
    const std::array<double, 3> at = target.position();
    if (name == "x" || name == "y" || name == "z") {
        return Value{at[name == "x" ? 0 : name == "y" ? 1 : 2]};
    }
    if (name == "pos") {
        return Value{Value::Map{{"x", Value{at[0]}}, {"y", Value{at[1]}}, {"z", Value{at[2]}}}};
    }
    if (name == "yaw" || name == "pitch") {
        return Value{static_cast<double>(facing(target)[name == "yaw" ? 0 : 1])};
    }
    if (name == "health") {
        return Value{target.entity != nullptr ? target.entity->health() : target.player->health()};
    }
    if (name == "on_ground") {
        return Value{target.entity != nullptr ? target.entity->onGround() : target.player->onGround()};
    }
    if (target.entity == nullptr) {
        return std::nullopt;
    }
    if (name == "age") {
        return Value{static_cast<std::int64_t>(target.entity->age())};
    }
    if (name == "width") {
        return Value{target.entity->width()};
    }
    if (name == "height") {
        return Value{target.entity->height()};
    }
    if (name == "velocity") {
        const std::array<double, 3> motion = target.entity->velocity();
        return Value{Value::Map{{"x", Value{motion[0]}}, {"y", Value{motion[1]}}, {"z", Value{motion[2]}}}};
    }
    return std::nullopt;
}

void entityGet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> name = c.memory().string(c.i32(1), c.i32(2));
    Target* target = nullptr;
    const std::int32_t status = entities::resolve(state, c.i32(0), target);
    if (status != abi::kOk || !name) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    if (const std::optional<Value> base = baseValue(*target, *name); base.has_value()) {
        c.ret(putValue(c.memory(), c.i32(3), c.i32(4), *base));
        return;
    }
    const std::optional<PluginEntityValue> found =
        target->entity != nullptr ? target->entity->property(*name) : target->player->property(*name);
    if (!found.has_value()) {
        c.ret(abi::kErrAbsent);
        return;
    }
    c.ret(putValue(c.memory(), c.i32(3), c.i32(4), entities::fromEntityValue(*name, *found)));
}

[[nodiscard]] std::int32_t setVelocityFrom(Target& target, const Value& value) {
    const Value* x = value.find("x");
    const Value* y = value.find("y");
    const Value* z = value.find("z");
    const auto number = [](const Value* one) -> std::optional<double> {
        if (one != nullptr && one->asFloat() != nullptr) {
            return *one->asFloat();
        }
        if (one != nullptr && one->asInt() != nullptr) {
            return static_cast<double>(*one->asInt());
        }
        return std::nullopt;
    };
    if (target.entity == nullptr || !number(x) || !number(y) || !number(z)) {
        return abi::kErrInvalid;
    }
    return target.entity->setVelocity(*number(x), *number(y), *number(z)) ? abi::kOk : abi::kErrInvalid;
}

void entitySet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(1), c.i32(2));
    const std::expected<Value, std::int32_t> value = readValue(memory, c.i32(3), c.i32(4));
    Target* target = nullptr;
    std::int32_t status = entities::resolve(state, c.i32(0), target);
    if (status == abi::kOk && (!name || !value)) {
        status = abi::kErrInvalid;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    if (*name == "velocity") {
        c.ret(setVelocityFrom(*target, *value));
        return;
    }
    if (*name == "yaw" || *name == "pitch") {
        const double angle = value->asFloat() != nullptr ? *value->asFloat()
                             : value->asInt() != nullptr ? static_cast<double>(*value->asInt())
                                                         : 0.0;
        const std::array<float, 2> now = facing(*target);
        const auto yaw = static_cast<float>(*name == "yaw" ? angle : now[0]);
        const auto pitch = static_cast<float>(*name == "pitch" ? angle : now[1]);
        const bool turned = target->entity != nullptr
                                ? target->entity->setRotation(yaw, pitch)
                                : target->player->teleport(target->player->world(), target->player->x(),
                                                           target->player->y(), target->player->z(), yaw, pitch);
        c.ret(turned ? abi::kOk : abi::kErrInvalid);
        return;
    }
    const std::expected<PluginEntityValue, std::int32_t> converted = entities::toEntityValue(*value);
    if (!converted) {
        c.ret(converted.error());
        return;
    }
    const bool known =
        (target->entity != nullptr ? target->entity->property(*name) : target->player->property(*name)).has_value();
    if (!known && baseValue(*target, *name) == std::nullopt) {
        c.ret(abi::kErrAbsent);
        return;
    }
    const bool wrote = target->entity != nullptr ? target->entity->setProperty(*name, *converted)
                                                 : target->player->setProperty(*name, *converted);
    c.ret(wrote ? abi::kOk : abi::kErrInvalid);
}

void entityCall(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> operation = memory.string(c.i32(1), c.i32(2));
    Target* target = nullptr;
    std::int32_t status = entities::resolve(state, c.i32(0), target);
    if (status == abi::kOk && !operation) {
        status = abi::kErrInvalid;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::expected<std::vector<PluginEntityValue>, std::int32_t> arguments =
        entities::toArguments(memory, c.i32(3), c.i32(4));
    if (!arguments) {
        c.ret(arguments.error());
        return;
    }
    const std::optional<PluginEntityValue> result = target->entity != nullptr
                                                        ? target->entity->invoke(*operation, *arguments)
                                                        : target->player->invoke(*operation, *arguments);
    if (!result.has_value()) {
        c.ret(abi::kErrAbsent);
        return;
    }
    c.ret(putValue(memory, c.i32(5), c.i32(6), entities::fromEntityValue(*operation, *result)));
}

void entityProperties(void* user, abi::Call& c) {
    Target* target = nullptr;
    const std::int32_t status = entities::resolve(stateOf(user), c.i32(0), target);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    std::vector<EntityName> names;
    for (const std::string_view base : kBaseNames) {
        names.push_back({std::string{base}, baseWritable(base)});
    }
    if (target->entity != nullptr) {
        for (EntityName& name : target->entity->propertyNames()) {
            names.push_back(std::move(name));
        }
    } else {
        for (const EntityPropertySpec& spec : kPlayerProperties) {
            names.push_back({std::string{spec.name}, spec.writable});
        }
    }
    std::set<std::string> seen;
    Value::List list;
    for (const EntityName& name : names) {
        if (seen.insert(name.name).second) {
            list.emplace_back(Value::Map{{"name", Value{name.name}}, {"writable", Value{name.writable}}});
        }
    }
    c.ret(putValue(c.memory(), c.i32(1), c.i32(2), Value{std::move(list)}));
}

void entityOperations(void* user, abi::Call& c) {
    Target* target = nullptr;
    const std::int32_t status = entities::resolve(stateOf(user), c.i32(0), target);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    std::vector<std::string> names;
    if (target->entity != nullptr) {
        names = target->entity->operationNames();
    } else {
        for (const EntityOperationSpec& spec : kPlayerOperations) {
            names.emplace_back(spec.name);
        }
    }
    std::ranges::sort(names);
    names.erase(std::ranges::unique(names).begin(), names.end());
    Value::List list;
    for (const std::string& name : names) {
        Value::List columns;
        for (const std::string_view column : entities::columnsOf(name)) {
            columns.emplace_back(std::string{column});
        }
        list.emplace_back(Value::Map{{"name", Value{name}}, {"columns", Value{std::move(columns)}}});
    }
    c.ret(putValue(c.memory(), c.i32(1), c.i32(2), Value{std::move(list)}));
}

void entitySnapshot(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Target* target = nullptr;
    const std::int32_t status = entities::resolve(state, c.i32(0), target);
    if (status != abi::kOk || target->entity == nullptr) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    const std::shared_ptr<PluginWorldRef> world = entities::worldNamed(state, target->world());
    const std::optional<std::vector<std::uint8_t>> bytes =
        world == nullptr ? std::nullopt : world->entityNbt(target->id());
    if (!bytes.has_value()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    c.ret(c.memory().put(c.i32(1), c.i32(2), {reinterpret_cast<const char*>(bytes->data()), bytes->size()}));
}

void entityRestore(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const auto bytes =
        memory.range(static_cast<std::uint32_t>(c.i32(1)), static_cast<std::uint32_t>(std::max(0, c.i32(2))));
    Target* target = nullptr;
    std::int32_t status = entities::resolve(state, c.i32(0), target);
    if (status == abi::kOk && (!bytes || c.i32(2) <= 0 || target->entity == nullptr ||
                               !wire::nbt::nbtIO::read(std::span<const std::uint8_t>{bytes->data(), bytes->size()}))) {
        status = abi::kErrInvalid;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::shared_ptr<PluginWorldRef> world = entities::worldNamed(state, target->world());
    c.ret(world != nullptr && world->loadEntityNbt(target->id(), {bytes->data(), bytes->size()}) ? abi::kOk
                                                                                                 : abi::kErrInvalid);
}

[[nodiscard]] bool readCategories(const Value* table, std::array<std::int32_t, 8>& into) {
    if (table == nullptr) {
        return true;
    }
    if (table->asMap() == nullptr) {
        return false;
    }
    for (std::size_t at = 0; at < kCategories.size(); ++at) {
        if (const Value* one = table->find(kCategories[at]); one != nullptr) {
            if (one->asInt() == nullptr) {
                return false;
            }
            into[at] = static_cast<std::int32_t>(*one->asInt());
        }
    }
    return true;
}

[[nodiscard]] Value categoriesOf(const std::array<std::int32_t, 8>& values) {
    Value::Map changed;
    for (std::size_t at = 0; at < kCategories.size(); ++at) {
        if (values[at] >= 0) {
            changed.push_back({std::string{kCategories[at]}, Value{static_cast<std::int64_t>(values[at])}});
        }
    }
    return Value{std::move(changed)};
}

void entitySpawnRules(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(0), c.i32(1));
    std::expected<Value, std::int32_t> rules = Value{Value::Map{}};
    if (c.i32(3) != 0) {
        rules = readValue(memory, c.i32(2), c.i32(3));
    }
    if (!name || !rules || rules->asMap() == nullptr) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::shared_ptr<PluginWorldRef> world = entities::worldNamed(state, *name);
    PluginWorldEnv* env = world == nullptr ? nullptr : world->env();
    if (env == nullptr) {
        c.ret(abi::kErrAbsent);
        return;
    }
    PluginSpawnTunables tunables = env->spawnTunables();
    bool changed = !rules->asMap()->empty();
    if (!readCategories(rules->find("limits"), tunables.limits) ||
        !readCategories(rules->find("ticks_per"), tunables.ticksPer)) {
        c.ret(abi::kErrInvalid);
        return;
    }
    for (const auto& [key, flag] :
         {std::pair{"allow_monsters", &tunables.allowMonsters}, std::pair{"allow_animals", &tunables.allowAnimals}}) {
        if (const Value* one = rules->find(key); one != nullptr) {
            if (one->asBool() == nullptr) {
                c.ret(abi::kErrInvalid);
                return;
            }
            *flag = *one->asBool();
        }
    }
    if (changed) {
        env->setSpawnTunables(tunables);
        tunables = env->spawnTunables();
    }
    c.ret(putValue(memory, c.i32(4), c.i32(5),
                   Value{Value::Map{{"limits", categoriesOf(tunables.limits)},
                                    {"ticks_per", categoriesOf(tunables.ticksPer)},
                                    {"allow_monsters", Value{tunables.allowMonsters}},
                                    {"allow_animals", Value{tunables.allowAnimals}}}}));
}

void entityEventEntity(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Context* context = nullptr;
    const std::int32_t status = state.check(c.i32(0), Context::Kind::Event, context);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const PluginEvent* event = context->event;
    if (event == nullptr || event->type().indexOf("entity_id") < 0 || event->type().indexOf("world") < 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const auto* id = std::get_if<std::int64_t>(&event->get("entity_id"));
    const auto* worldName = std::get_if<std::string>(&event->get("world"));
    const std::shared_ptr<PluginWorldRef> world =
        worldName == nullptr ? nullptr : entities::worldNamed(state, *worldName);
    const std::shared_ptr<PluginEntityRef> found =
        id == nullptr || world == nullptr ? nullptr : world->entityById(static_cast<std::int32_t>(*id));
    c.ret(found == nullptr || !found->alive() ? abi::kErrAbsent : entities::table(state).handleFor(found));
}

void entityAiAnswer(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Context* context = nullptr;
    const std::int32_t status = state.check(c.i32(0), Context::Kind::Event, context);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    PluginEvent* event = context->event;
    if (event == nullptr || (event->type().name() != "entity.goal" && event->type().name() != "entity.behavior")) {
        c.ret(abi::kErrInvalid);
        return;
    }
    event->set("result", c.i32(1) != 0);
    c.ret(abi::kOk);
}
} // namespace

void addEntitiesExtraImports(State& state, std::vector<abi::HostImport>& out) {
    void* user = &state;
    out.insert(out.end(), {
                              {"entity_get", entityGet, user},
                              {"entity_set", entitySet, user},
                              {"entity_call", entityCall, user},
                              {"entity_properties", entityProperties, user},
                              {"entity_operations", entityOperations, user},
                              {"entity_snapshot", entitySnapshot, user},
                              {"entity_restore", entityRestore, user},
                              {"entity_spawn_rules", entitySpawnRules, user},
                              {"entity_event_entity", entityEventEntity, user},
                              {"entity_ai_answer", entityAiAnswer, user},
                          });
}
} // namespace basalt::server::plugins::wasm
