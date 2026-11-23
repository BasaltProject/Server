#include "WasmEntities.hpp"
#include "WasmItemState.hpp"

#include <basalt/server/entity/LevelValue.hpp>
#include <basalt/server/registry/CreativeTabs.hpp>

namespace basalt::server::plugins::wasm {
namespace {
using namespace items;

[[nodiscard]] std::int32_t resolve(State& state, std::int32_t handle, std::unique_ptr<Access>& out) {
    Target* target = nullptr;
    const std::int32_t status = itemsOf(state).inventories.find(handle, target);
    if (status != abi::kOk) {
        return status;
    }
    if (target->kind == Kind::Block) {
        out = openBlock(*target->world, target->at);
        return out != nullptr ? abi::kOk : abi::kErrStale;
    }
    if (target->kind == Kind::Entity) {
        return abi::kErrStale;
    }
    if (!target->player->online()) {
        return abi::kErrStale;
    }
    if (target->kind == Kind::Player) {
        out = std::make_unique<PlayerAccess>(*target->player);
    } else if (target->kind == Kind::Ender) {
        out = std::make_unique<EnderAccess>(*target->player);
    } else {
        PluginWindow* window = target->player->window();
        if (window == nullptr || window->windowInfo().containerId != target->container) {
            return abi::kErrStale;
        }
        out = std::make_unique<WindowAccess>(*window);
    }
    return abi::kOk;
}

[[nodiscard]] bool inRange(Access& access, std::int32_t slot) {
    return (slot >= 0 && slot < access.size()) || (slot == -1 && access.hasCursor());
}

[[nodiscard]] std::int32_t added(State& state, Target target) {
    const std::int32_t handle = itemsOf(state).inventories.add(std::move(target));
    return handle != 0 ? handle : abi::kErrInvalid;
}

void ofPlayer(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    c.ret(status != abi::kOk ? status : added(state, Target{.kind = Kind::Player, .player = player}));
}

void ofEnder(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    c.ret(status != abi::kOk ? status : added(state, Target{.kind = Kind::Ender, .player = player}));
}

void ofWindow(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    PluginWindow* window = status == abi::kOk ? player->window() : nullptr;
    if (status != abi::kOk || window == nullptr || window->windowInfo().containerId == 0) {
        c.ret(status != abi::kOk ? status : abi::kErrAbsent);
        return;
    }
    c.ret(added(state, Target{.kind = Kind::Window, .player = player, .container = window->windowInfo().containerId}));
}

void ofBlock(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> name = c.memory().string(c.i32(0), c.i32(1));
    if (!name) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginWorlds& worlds = state.host->services().worlds();
    const std::shared_ptr<PluginWorldRef> world = name->empty() ? worlds.defaultWorld() : worlds.byName(*name);
    const std::array<std::int32_t, 3> at{c.i32(2), c.i32(3), c.i32(4)};
    if (world == nullptr || openBlock(*world, at) == nullptr) {
        c.ret(abi::kErrAbsent);
        return;
    }
    c.ret(added(state, Target{.kind = Kind::Block, .world = world, .at = at}));
}

void ofEntity(void* user, abi::Call& c) {
    State& state = stateOf(user);
    entities::Target* target = nullptr;
    const std::int32_t status = entities::resolve(state, c.i32(0), target);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::shared_ptr<PluginPlayerRef> player = entities::playerOf(state, *target);
    c.ret(player == nullptr ? abi::kErrAbsent : added(state, Target{.kind = Kind::Player, .player = player}));
}

void release(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Target* target = nullptr;
    const std::int32_t status = itemsOf(state).inventories.find(c.i32(0), target);
    if (status == abi::kOk) {
        static_cast<void>(itemsOf(state).inventories.remove(c.i32(0)));
    }
    c.ret(status);
}

void inventorySize(void* user, abi::Call& c) {
    std::unique_ptr<Access> access;
    const std::int32_t status = resolve(stateOf(user), c.i32(0), access);
    c.ret(status != abi::kOk ? status : access->size());
}

void inventoryGet(void* user, abi::Call& c) {
    std::unique_ptr<Access> access;
    std::int32_t status = resolve(stateOf(user), c.i32(0), access);
    if (status == abi::kOk && !inRange(*access, c.i32(1))) {
        status = abi::kErrInvalid;
    }
    c.ret(status != abi::kOk ? status : putValue(c.memory(), c.i32(2), c.i32(3), toValue(access->get(c.i32(1)))));
}

void inventorySet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::unique_ptr<Access> access;
    const std::expected<Slot, std::int32_t> item = readStack(state, c.memory(), c.i32(2), c.i32(3));
    std::int32_t status = resolve(state, c.i32(0), access);
    if (status == abi::kOk && (!item.has_value() || !inRange(*access, c.i32(1)))) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        access->set(c.i32(1), *item);
        access->commit();
    }
    c.ret(status);
}

void inventoryAdd(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::unique_ptr<Access> access;
    const std::expected<Slot, std::int32_t> item = readStack(state, c.memory(), c.i32(1), c.i32(2));
    std::int32_t status = resolve(state, c.i32(0), access);
    if (status == abi::kOk && (!item.has_value() || !item->has_value())) {
        status = abi::kErrInvalid;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::int32_t left = access->add(**item);
    access->commit();
    c.ret(left);
}

void inventoryRemove(void* user, abi::Call& c) {
    std::unique_ptr<Access> access;
    const std::optional<std::string_view> id = c.memory().string(c.i32(1), c.i32(2));
    std::int32_t status = resolve(stateOf(user), c.i32(0), access);
    if (status == abi::kOk && (!id || id->empty() || c.i32(3) < 1)) {
        status = abi::kErrInvalid;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::int32_t taken = access->remove(qualified(*id), c.i32(3));
    access->commit();
    c.ret(taken);
}

void inventoryCount(void* user, abi::Call& c) {
    std::unique_ptr<Access> access;
    const std::optional<std::string_view> id = c.memory().string(c.i32(1), c.i32(2));
    std::int32_t status = resolve(stateOf(user), c.i32(0), access);
    if (status == abi::kOk && (!id || id->empty())) {
        status = abi::kErrInvalid;
    }
    c.ret(status != abi::kOk ? status : access->count(qualified(*id)));
}

void inventoryClear(void* user, abi::Call& c) {
    std::unique_ptr<Access> access;
    const std::int32_t status = resolve(stateOf(user), c.i32(0), access);
    if (status == abi::kOk) {
        access->clear();
        access->commit();
    }
    c.ret(status);
}

void inventoryContents(void* user, abi::Call& c) {
    std::unique_ptr<Access> access;
    const std::int32_t status = resolve(stateOf(user), c.i32(0), access);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    Value::List list;
    for (std::int32_t slot = 0; slot < access->size(); ++slot) {
        if (const Slot held = access->get(slot); held.has_value()) {
            list.push_back(
                Value{Value::Map{{"slot", Value{static_cast<std::int64_t>(slot)}}, {"item", toValue(held)}}});
        }
    }
    c.ret(putValue(c.memory(), c.i32(1), c.i32(2), Value{std::move(list)}));
}

void heldSlot(void* user, abi::Call& c) {
    std::unique_ptr<Access> access;
    const std::int32_t status = resolve(stateOf(user), c.i32(0), access);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::optional<std::int32_t> held = access->heldSlot();
    c.ret(held.has_value() ? *held : abi::kErrAbsent);
}

void setHeldSlot(void* user, abi::Call& c) {
    std::unique_ptr<Access> access;
    std::int32_t status = resolve(stateOf(user), c.i32(0), access);
    if (status == abi::kOk && !access->heldSlot().has_value()) {
        status = abi::kErrAbsent;
    } else if (status == abi::kOk && (c.i32(1) < 0 || c.i32(1) > 8)) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        static_cast<void>(access->setHeldSlot(c.i32(1)));
    }
    c.ret(status);
}

void cooldown(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    const std::optional<std::string_view> id = c.memory().string(c.i32(1), c.i32(2));
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status == abi::kOk && (!id || id->empty())) {
        status = abi::kErrInvalid;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::optional<PluginEntityValue> left = player->invoke("cooldown", {std::string{*id}});
    const auto* ticks = left.has_value() ? std::get_if<std::int64_t>(&*left) : nullptr;
    c.ret(ticks == nullptr ? 0 : static_cast<std::int32_t>(*ticks));
}

void setCooldown(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    const std::optional<std::string_view> id = c.memory().string(c.i32(1), c.i32(2));
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status == abi::kOk && (!id || id->empty())) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        static_cast<void>(player->invoke("set_cooldown", {std::string{*id}, std::int64_t{c.i32(3)}}));
    }
    c.ret(status);
}

[[nodiscard]] Value tagMap(const std::vector<std::string>& rows) {
    Value::Map map;
    for (const std::string& row : rows) {
        Value::List members;
        std::size_t at = row.find('|');
        const std::string tag = row.substr(0, at);
        while (at != std::string::npos) {
            const std::size_t next = row.find('|', at + 1);
            members.emplace_back(row.substr(at + 1, next == std::string::npos ? next : next - at - 1));
            at = next;
        }
        map.push_back({tag, Value{std::move(members)}});
    }
    return Value{std::move(map)};
}

[[nodiscard]] std::optional<Value> fromBytes(const std::optional<std::vector<std::uint8_t>>& bytes,
                                             std::string_view under = {}) {
    const std::optional<NbtCompound> compound = bytes.has_value() ? compoundFromBytes(*bytes) : std::nullopt;
    if (!compound.has_value()) {
        return std::nullopt;
    }
    if (under.empty()) {
        return nbtToValue(NbtTag::ofCompound(*compound));
    }
    const NbtTag* inner = compound->find(under);
    return inner == nullptr ? std::optional<Value>{Value{Value::List{}}} : std::optional{nbtToValue(*inner)};
}

void registryGet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> kind = memory.string(c.i32(0), c.i32(1));
    const std::optional<std::string_view> key = memory.string(c.i32(2), c.i32(3));
    if (!kind || !key) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginInventories& inventories = state.host->services().inventories();
    std::optional<Value> answer;
    if (*kind == "item") {
        answer = fromBytes(inventories.itemData(qualified(*key)));
    } else if (*kind == "items") {
        answer = strings(state.host->services().worlds().itemIds());
    } else if (*kind == "creative_tab") {
        const auto ids = registry::creativeTabItems(*key);
        answer = strings(std::vector<std::string>(ids.begin(), ids.end()));
    } else if (*kind == "enchantment") {
        answer = fromBytes(inventories.enchantmentData(qualified(*key)));
    } else if (*kind == "enchantments") {
        answer = strings(inventories.enchantmentIds());
    } else if (*kind == "tags") {
        answer = tagMap(inventories.registryTags(*key));
    } else if (*kind == "mob_effects") {
        answer = fromBytes(inventories.mobEffects(), "effects");
    } else if (*kind == "potions") {
        answer = fromBytes(inventories.potions(), "potions");
    } else {
        c.ret(abi::kErrInvalid);
        return;
    }
    c.ret(answer.has_value() ? putValue(memory, c.i32(4), c.i32(5), *answer) : abi::kErrAbsent);
}

constexpr int kMaxLevelValueDepth = 16;

[[nodiscard]] double numberOf(const Value* value, double fallback) {
    if (value == nullptr) {
        return fallback;
    }
    if (const double* real = value->asFloat()) {
        return *real;
    }
    const std::int64_t* whole = value->asInt();
    return whole != nullptr ? static_cast<double>(*whole) : fallback;
}

void encodeLevelValue(const Value& spec, std::vector<double>& program, int depth) {
    using Op = entity::levelValue::Op;
    const auto emit = [&program](Op op) { program.push_back(static_cast<double>(static_cast<int>(op))); };
    if (depth > kMaxLevelValueDepth || spec.asMap() == nullptr) {
        emit(Op::Constant);
        program.push_back(numberOf(&spec, 0.0));
        return;
    }
    const Value* typeValue = spec.find("type");
    std::string type = typeValue != nullptr && typeValue->asString() != nullptr ? *typeValue->asString() : "constant";
    if (type.starts_with("minecraft:")) {
        type = type.substr(10);
    }
    const auto child = [&](std::string_view key) {
        const Value* found = spec.find(key);
        encodeLevelValue(found != nullptr ? *found : Value{}, program, depth + 1);
    };
    if (type == "linear") {
        emit(Op::Linear);
        program.push_back(numberOf(spec.find("base"), 0.0));
        program.push_back(numberOf(spec.find("per_level_above_first"), 0.0));
    } else if (type == "levels_squared") {
        emit(Op::LevelsSquared);
        program.push_back(numberOf(spec.find("added"), 0.0));
    } else if (type == "clamped") {
        emit(Op::Clamped);
        program.push_back(numberOf(spec.find("min"), 0.0));
        program.push_back(numberOf(spec.find("max"), 0.0));
        child("value");
    } else if (type == "fraction") {
        emit(Op::Fraction);
        child("numerator");
        child("denominator");
    } else if (type == "exponent") {
        emit(Op::Exponent);
        child("base");
        child("power");
    } else if (type == "lookup") {
        emit(Op::Lookup);
        const Value* values = spec.find("values");
        const Value::List* list = values != nullptr ? values->asList() : nullptr;
        program.push_back(list != nullptr ? static_cast<double>(list->size()) : 0.0);
        for (std::size_t at = 0; list != nullptr && at < list->size(); ++at) {
            program.push_back(numberOf(&(*list)[at], 0.0));
        }
        child("fallback");
    } else {
        emit(Op::Constant);
        program.push_back(numberOf(spec.find("value"), 0.0));
    }
}

void enchantLevelValue(void*, abi::Call& c) {
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(0), c.i32(1));
    std::vector<double> program;
    if (spec.has_value()) {
        encodeLevelValue(*spec, program, 0);
    }
    c.ret(spec.has_value() ? static_cast<double>(entity::levelValue::evaluate(program, c.i32(2))) : 0.0);
}
} // namespace

void addItemsImports(State& state, std::vector<abi::HostImport>& out) {
    void* user = &state;
    out.insert(out.end(),
               {
                   {"inventory_of_player", ofPlayer, user},     {"inventory_of_block", ofBlock, user},
                   {"inventory_of_window", ofWindow, user},     {"inventory_of_ender", ofEnder, user},
                   {"inventory_of_entity", ofEntity, user},     {"inventory_release", release, user},
                   {"inventory_size", inventorySize, user},     {"inventory_get", inventoryGet, user},
                   {"inventory_set", inventorySet, user},       {"inventory_add", inventoryAdd, user},
                   {"inventory_remove", inventoryRemove, user}, {"inventory_count", inventoryCount, user},
                   {"inventory_clear", inventoryClear, user},   {"inventory_contents", inventoryContents, user},
                   {"inventory_held_slot", heldSlot, user},     {"inventory_set_held_slot", setHeldSlot, user},
                   {"item_cooldown", cooldown, user},           {"item_set_cooldown", setCooldown, user},
                   {"registry_get", registryGet, user},         {"enchantment_level_value", enchantLevelValue, user},
               });
}
} // namespace basalt::server::plugins::wasm
