#include "WasmItemState.hpp"

#include <basalt/server/plugins/PluginLoot.hpp>

namespace basalt::server::plugins::wasm {
namespace {
using namespace items;

[[nodiscard]] PluginLoot& loot(State& state) { return state.host->services().loot(); }

[[nodiscard]] std::optional<double> numberOf(const Value* value) {
    if (value == nullptr || value->isNull()) {
        return std::nullopt;
    }
    if (value->asFloat() != nullptr) {
        return *value->asFloat();
    }
    if (value->asInt() != nullptr) {
        return static_cast<double>(*value->asInt());
    }
    return std::nullopt;
}

/// The `world` key of a spec; empty means the default world, as the header says.
[[nodiscard]] std::string worldOf(State& state, const Value& map) {
    const Value* found = map.find("world");
    if (found != nullptr && found->asString() != nullptr && !found->asString()->empty()) {
        return *found->asString();
    }
    const std::shared_ptr<PluginWorldRef> world = state.host->services().worlds().defaultWorld();
    return world != nullptr ? world->name() : std::string{};
}

[[nodiscard]] std::string textOf(const Value& map, std::string_view key) {
    const Value* found = map.find(key);
    return found != nullptr && found->asString() != nullptr ? *found->asString() : std::string{};
}

[[nodiscard]] std::int64_t seedOf(const Value& map) {
    const Value* found = map.find("seed");
    return found != nullptr && found->asInt() != nullptr ? *found->asInt() : 0;
}

/// A spec is null, an empty map or a map; anything else is bad.
[[nodiscard]] std::expected<Value, std::int32_t> specOf(const abi::GuestMemory& memory, std::int32_t ptr,
                                                        std::int32_t len) {
    if (len == 0) {
        return Value{Value::Map{}};
    }
    std::expected<Value, std::int32_t> spec = readValue(memory, ptr, len);
    if (!spec.has_value()) {
        return spec;
    }
    if (spec->isNull()) {
        return Value{Value::Map{}};
    }
    return spec->asMap() == nullptr ? std::unexpected(abi::kErrInvalid) : spec;
}

[[nodiscard]] PluginLootRoll rollOf(State& state, const Value& spec) {
    PluginLootRoll roll;
    roll.world = worldOf(state, spec);
    roll.x = numberOf(spec.find("x")).value_or(0.0);
    roll.y = numberOf(spec.find("y")).value_or(0.0);
    roll.z = numberOf(spec.find("z")).value_or(0.0);
    roll.luck = static_cast<float>(numberOf(spec.find("luck")).value_or(0.0));
    roll.seed = seedOf(spec);
    roll.looter = textOf(spec, "looter");
    return roll;
}

[[nodiscard]] Value lootValue(const PluginLootItem& stack) {
    PluginItem item{.id = stack.item, .count = std::max(stack.count, 1)};
    if (const std::optional<NbtCompound> compound = [&]() -> std::optional<NbtCompound> {
            if (stack.components.empty()) {
                return std::nullopt;
            }
            const std::expected<NbtTag, wire::nbt::NbtError> parsed = wire::nbt::snbt::parse(stack.components);
            const NbtCompound* root = parsed.has_value() ? parsed->get<NbtCompound>() : nullptr;
            return root == nullptr ? std::nullopt : std::optional<NbtCompound>{*root};
        }();
        compound.has_value()) {
        NbtCompound shaped;
        shaped.putString("id", item.id);
        shaped.putInt("count", item.count);
        shaped.set("components", NbtTag::ofCompound(*compound));
        return toValue(itemOfCompound(shaped));
    }
    return toValue(item);
}

[[nodiscard]] std::optional<PluginLootItem> stackOf(State& state, const Value* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    const std::optional<PluginItem> item =
        fromValue(*value, [&state](const std::string& id) { return knownItem(state, id); });
    if (!item.has_value()) {
        return std::nullopt;
    }
    const NbtCompound components = mergedComponents(*item);
    return PluginLootItem{
        .item = item->id,
        .count = item->count,
        .components = components.empty() ? std::string{} : wire::nbt::snbt::stringify(NbtTag::ofCompound(components))};
}

[[nodiscard]] Value stacks(const std::vector<PluginLootItem>& rolled) {
    Value::List list;
    for (const PluginLootItem& stack : rolled) {
        list.push_back(lootValue(stack));
    }
    return Value{std::move(list)};
}

void tables(void* user, abi::Call& c) {
    c.ret(putValue(c.memory(), c.i32(0), c.i32(1), strings(loot(stateOf(user)).tables())));
}

void roll(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> table = c.memory().string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> spec = specOf(c.memory(), c.i32(2), c.i32(3));
    if (!table || !spec.has_value() || table->empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const auto rolled = loot(state).roll(*table, rollOf(state, *spec));
    c.ret(rolled.has_value() ? putValue(c.memory(), c.i32(4), c.i32(5), stacks(*rolled)) : abi::kErrAbsent);
}

void rollJson(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> json = c.memory().string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> spec = specOf(c.memory(), c.i32(2), c.i32(3));
    if (!json || !spec.has_value() || json->empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const auto rolled = loot(state).rollJson(*json, rollOf(state, *spec));
    c.ret(rolled.has_value() ? putValue(c.memory(), c.i32(4), c.i32(5), stacks(*rolled)) : abi::kErrAbsent);
}

void number(void* user, abi::Call& c) {
    const std::optional<std::string_view> json = c.memory().string(c.i32(0), c.i32(1));
    if (!json || json->empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const bool asFloat = c.i32(2) != 0;
    const std::optional<double> answer = loot(stateOf(user)).number(*json, asFloat, c.i64(3));
    c.ret(!answer.has_value() ? abi::kErrAbsent
                              : putValue(c.memory(), c.i32(4), c.i32(5),
                                         asFloat ? Value{*answer} : Value{static_cast<std::int64_t>(*answer)}));
}

void apply(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::expected<Value, std::int32_t> spec = specOf(c.memory(), c.i32(0), c.i32(1));
    const std::optional<PluginLootItem> stack = spec.has_value() ? stackOf(state, spec->find("item")) : std::nullopt;
    const std::string functions = spec.has_value() ? textOf(*spec, "functions") : std::string{};
    if (!spec.has_value() || !stack.has_value() || functions.empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginLootApply how{.world = worldOf(state, *spec),
                        .x = numberOf(spec->find("x")).value_or(0.0),
                        .y = numberOf(spec->find("y")).value_or(0.0),
                        .z = numberOf(spec->find("z")).value_or(0.0),
                        .seed = seedOf(*spec)};
    const std::optional<PluginLootItem> made = loot(state).apply(functions, *stack, how);
    c.ret(made.has_value() ? putValue(c.memory(), c.i32(2), c.i32(3), lootValue(*made)) : abi::kErrAbsent);
}

void enchant(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::expected<Value, std::int32_t> spec = specOf(c.memory(), c.i32(0), c.i32(1));
    const std::optional<PluginLootItem> stack = spec.has_value() ? stackOf(state, spec->find("item")) : std::nullopt;
    if (!spec.has_value() || !stack.has_value()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginLootEnchant how;
    how.world = worldOf(state, *spec);
    if (const std::string options = textOf(*spec, "options"); !options.empty()) {
        how.options = options;
    }
    if (const Value* levels = spec->find("levels"); levels != nullptr && levels->asInt() != nullptr) {
        how.levels = static_cast<std::int32_t>(*levels->asInt());
    }
    how.seed = seedOf(*spec);
    const std::optional<PluginLootItem> made = loot(state).enchant(*stack, how);
    c.ret(made.has_value() ? putValue(c.memory(), c.i32(2), c.i32(3), lootValue(*made)) : abi::kErrAbsent);
}

void trades(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> profession = c.memory().string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> spec = specOf(c.memory(), c.i32(3), c.i32(4));
    if (!profession || !spec.has_value() || profession->empty() || c.i32(2) < 1 || c.i32(2) > 5) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginTradeRoll how;
    how.world = worldOf(state, *spec);
    how.x = numberOf(spec->find("x")).value_or(0.0);
    how.y = numberOf(spec->find("y")).value_or(0.0);
    how.z = numberOf(spec->find("z")).value_or(0.0);
    how.variant = textOf(*spec, "variant");
    how.seed = seedOf(*spec);
    if (const Value* amount = spec->find("amount"); amount != nullptr && amount->asInt() != nullptr) {
        how.amount = static_cast<std::int32_t>(*amount->asInt());
    }
    if (const Value* listed = spec->find("listed"); listed != nullptr && listed->asList() != nullptr) {
        for (const Value& id : *listed->asList()) {
            if (id.asString() == nullptr) {
                c.ret(abi::kErrInvalid);
                return;
            }
            how.listed.push_back(*id.asString());
        }
    }
    constexpr std::string_view kNamespace = "minecraft:";
    const std::string_view bare =
        profession->starts_with(kNamespace) ? profession->substr(kNamespace.size()) : *profession;
    const auto offers = loot(state).trades(bare, c.i32(2), how);
    if (!offers.has_value()) {
        c.ret(abi::kErrAbsent);
        return;
    }
    Value::List list;
    for (const PluginTradeOffer& offer : *offers) {
        list.push_back(Value{Value::Map{{"cost_a", lootValue(offer.costA)},
                                        {"cost_b", offer.costB.has_value() ? lootValue(*offer.costB) : Value{}},
                                        {"result", lootValue(offer.result)},
                                        {"max_uses", Value{static_cast<std::int64_t>(offer.maxUses)}},
                                        {"xp", Value{static_cast<std::int64_t>(offer.experience)}},
                                        {"price_multiplier", Value{static_cast<double>(offer.priceMultiplier)}}}});
    }
    c.ret(putValue(c.memory(), c.i32(5), c.i32(6), Value{std::move(list)}));
}
} // namespace

void addItemsLootImports(State& state, std::vector<abi::HostImport>& out) {
    void* user = &state;
    out.insert(out.end(), {{"loot_tables", tables, user},
                           {"loot_roll", roll, user},
                           {"loot_roll_json", rollJson, user},
                           {"loot_number", number, user},
                           {"loot_apply", apply, user},
                           {"loot_enchant", enchant, user},
                           {"trades_roll", trades, user}});
}
} // namespace basalt::server::plugins::wasm
