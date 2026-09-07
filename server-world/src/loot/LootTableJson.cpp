#include <basalt/server/world/loot/LootTableJson.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <charconv>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::loot::lootTableJson {
namespace {

using wire::common::JsonObject;
using wire::common::JsonValue;

/// A parse either produced the value or named what it could not read.
template <typename Value>
using Parsed = std::expected<Value, LootTableParseError>;

[[nodiscard]] LootTableParseError missing(std::string_view what) {
    return LootTableParseError{std::string{"missing or malformed "} + std::string{what}};
}

[[nodiscard]] const JsonObject* asObject(const JsonValue* value) {
    return value == nullptr ? nullptr : value->get<JsonObject>();
}

[[nodiscard]] const std::vector<JsonValue>* asArray(const JsonValue* value) {
    return value == nullptr ? nullptr : value->get<std::vector<JsonValue>>();
}

[[nodiscard]] std::optional<std::string> asString(const JsonValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const std::string* text = value->get<std::string>(); text != nullptr) {
        return *text;
    }
    return std::nullopt;
}

/// A JSON number as read by a schema that also accepts it written as a string.
[[nodiscard]] std::optional<double> asNumber(const JsonValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const double* real = value->get<double>(); real != nullptr) {
        return *real;
    }
    if (const std::int64_t* whole = value->get<std::int64_t>(); whole != nullptr) {
        return static_cast<double>(*whole);
    }
    if (const std::string* text = value->get<std::string>(); text != nullptr) {
        double parsed = 0.0;
        const char* first = text->data();
        const char* last = first + text->size();
        if (std::from_chars(first, last, parsed).ec == std::errc{}) {
            return parsed;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<bool> asBool(const JsonValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const bool* flag = value->get<bool>(); flag != nullptr) {
        return *flag;
    }
    if (const std::string* text = value->get<std::string>(); text != nullptr) {
        return *text == "true";
    }
    return std::nullopt;
}

[[nodiscard]] Parsed<NumberProvider> parseNumberProvider(const JsonValue& element);
[[nodiscard]] Parsed<LootCondition> parseCondition(const JsonObject& condition);
[[nodiscard]] Parsed<LootFunction> parseFunction(const JsonObject& function);
[[nodiscard]] Parsed<LootEntry> parseEntry(const JsonObject& entry);

Parsed<NumberProvider> parseNumberProvider(const JsonValue& element) {
    if (element.get<JsonObject>() == nullptr) {
        const std::optional<double> value = asNumber(&element);
        if (!value.has_value()) {
            return std::unexpected(missing("number provider value"));
        }
        return NumberProvider::constant(static_cast<float>(*value));
    }
    const JsonObject& object = *element.get<JsonObject>();
    const std::optional<std::string> type = asString(object.find("type"));
    if (!type.has_value()) {
        return std::unexpected(missing("number provider type"));
    }
    if (*type == "minecraft:constant") {
        const std::optional<double> value = asNumber(object.find("value"));
        if (!value.has_value()) {
            return std::unexpected(missing("constant value"));
        }
        return NumberProvider::constant(static_cast<float>(*value));
    }
    if (*type == "minecraft:uniform") {
        const JsonValue* min = object.find("min");
        const JsonValue* max = object.find("max");
        if (min == nullptr || max == nullptr) {
            return std::unexpected(missing("uniform bounds"));
        }
        Parsed<NumberProvider> low = parseNumberProvider(*min);
        if (!low.has_value()) {
            return low;
        }
        Parsed<NumberProvider> high = parseNumberProvider(*max);
        if (!high.has_value()) {
            return high;
        }
        return NumberProvider::uniform(std::move(*low), std::move(*high));
    }
    return std::unexpected(LootTableParseError{"Unsupported number provider type: " + *type});
}

[[nodiscard]] Parsed<std::vector<LootCondition>> parseConditions(const JsonValue* value) {
    std::vector<LootCondition> parsed;
    const std::vector<JsonValue>* array = asArray(value);
    if (array == nullptr) {
        return parsed;
    }
    for (const JsonValue& element : *array) {
        const JsonObject* object = element.get<JsonObject>();
        if (object == nullptr) {
            return std::unexpected(missing("loot condition"));
        }
        Parsed<LootCondition> condition = parseCondition(*object);
        if (!condition.has_value()) {
            return std::unexpected(condition.error());
        }
        parsed.push_back(std::move(*condition));
    }
    return parsed;
}

[[nodiscard]] Parsed<std::vector<LootFunction>> parseFunctions(const JsonValue* value) {
    std::vector<LootFunction> parsed;
    const std::vector<JsonValue>* array = asArray(value);
    if (array == nullptr) {
        return parsed;
    }
    for (const JsonValue& element : *array) {
        const JsonObject* object = element.get<JsonObject>();
        if (object == nullptr) {
            return std::unexpected(missing("loot function"));
        }
        Parsed<LootFunction> function = parseFunction(*object);
        if (!function.has_value()) {
            return std::unexpected(function.error());
        }
        parsed.push_back(std::move(*function));
    }
    return parsed;
}

// Conditions dispatch on `condition`, not `type`: `LootItemCondition.TYPED_CODEC`'s explicit
// dispatch key, unlike entries and number providers which use the default one.
Parsed<LootCondition> parseCondition(const JsonObject& condition) {
    const std::optional<std::string> type = asString(condition.find("condition"));
    if (!type.has_value()) {
        return std::unexpected(missing("loot condition type"));
    }
    if (*type == "minecraft:random_chance") {
        const JsonValue* chance = condition.find("chance");
        if (chance == nullptr) {
            return std::unexpected(missing("random chance"));
        }
        Parsed<NumberProvider> provider = parseNumberProvider(*chance);
        if (!provider.has_value()) {
            return std::unexpected(provider.error());
        }
        return LootCondition::randomChance(std::move(*provider));
    }
    if (*type == "minecraft:survives_explosion") {
        return LootCondition::survivesExplosion();
    }
    if (*type == "minecraft:match_tool") {
        return LootCondition::matchTool();
    }
    // parity: the conditions a mob's own table carries, about what died and what killed it.
    if (*type == "minecraft:entity_properties" || *type == "minecraft:damage_source_properties") {
        return LootCondition::entityProperties();
    }
    if (*type == "minecraft:killed_by_player") {
        return LootCondition::killedByPlayer();
    }
    if (*type == "minecraft:inverted") {
        const JsonObject* term = asObject(condition.find("term"));
        if (term == nullptr) {
            return std::unexpected(missing("inverted term"));
        }
        Parsed<LootCondition> inner = parseCondition(*term);
        if (!inner.has_value()) {
            return inner;
        }
        return LootCondition::inverted(std::move(*inner));
    }
    if (*type == "minecraft:any_of" || *type == "minecraft:all_of") {
        Parsed<std::vector<LootCondition>> terms = parseConditions(condition.find("terms"));
        if (!terms.has_value()) {
            return std::unexpected(terms.error());
        }
        return *type == "minecraft:any_of" ? LootCondition::anyOf(std::move(*terms))
                                           : LootCondition::allOf(std::move(*terms));
    }
    if (*type == "minecraft:random_chance_with_enchanted_bonus") {
        const std::optional<double> chance = asNumber(condition.find("unenchanted_chance"));
        return LootCondition::randomChanceWithEnchantedBonus(static_cast<float>(chance.value_or(0.0)));
    }
    return std::unexpected(LootTableParseError{"Unsupported loot condition type: " + *type});
}

// Functions dispatch on `function`, not `type`: the same explicit key conditions use.
Parsed<LootFunction> parseFunction(const JsonObject& function) {
    const std::optional<std::string> type = asString(function.find("function"));
    if (!type.has_value()) {
        return std::unexpected(missing("loot function type"));
    }
    if (*type == "minecraft:set_count") {
        const JsonValue* count = function.find("count");
        if (count == nullptr) {
            return std::unexpected(missing("set_count count"));
        }
        Parsed<NumberProvider> provider = parseNumberProvider(*count);
        if (!provider.has_value()) {
            return std::unexpected(provider.error());
        }
        return LootFunction::setCount(std::move(*provider), asBool(function.find("add")).value_or(false));
    }
    // parity: `furnace_smelt`, the cooked drop a mob killed by fire leaves, and the two that only
    // set a component: the item itself is right without them, so they are read and left alone.
    if (*type == "minecraft:furnace_smelt" || *type == "minecraft:set_potion" ||
        *type == "minecraft:set_ominous_bottle_amplifier") {
        return LootFunction::furnaceSmelt();
    }
    // parity: `enchanted_count_increase`, the looting bonus every mob's table carries.
    if (*type == "minecraft:enchanted_count_increase") {
        const JsonValue* count = function.find("count");
        if (count == nullptr) {
            return std::unexpected(missing("enchanted_count_increase count"));
        }
        Parsed<NumberProvider> provider = parseNumberProvider(*count);
        if (!provider.has_value()) {
            return std::unexpected(provider.error());
        }
        const std::optional<double> limit = asNumber(function.find("limit"));
        return LootFunction::enchantedCountIncrease(
            std::move(*provider),
            limit.has_value() ? std::optional<std::int32_t>{static_cast<std::int32_t>(*limit)} : std::nullopt);
    }
    // parity: `ApplyExplosionDecay`, which is what leaves a crater with only some of what it broke.
    // Refusing it left every table that carries it dropping nothing at all.
    if (*type == "minecraft:explosion_decay") {
        return LootFunction::explosionDecay();
    }
    // parity: the four that carry something off the block entity onto the stack. What they carry is
    // put on by the caller that knows the block, so the drop itself is right without them, and
    // refusing the whole table left the block dropping nothing.
    if (*type == "minecraft:copy_components" || *type == "minecraft:copy_custom_data" ||
        *type == "minecraft:copy_state" || *type == "minecraft:copy_name") {
        return LootFunction::copyFromBlock();
    }
    return std::unexpected(LootTableParseError{"Unsupported loot function type: " + *type});
}

Parsed<LootEntry> parseEntry(const JsonObject& entry) {
    Parsed<std::vector<LootCondition>> conditions = parseConditions(entry.find("conditions"));
    if (!conditions.has_value()) {
        return std::unexpected(conditions.error());
    }
    const std::optional<std::string> type = asString(entry.find("type"));
    if (!type.has_value()) {
        return std::unexpected(missing("loot entry type"));
    }
    if (*type == "minecraft:item") {
        const std::optional<std::string> name = asString(entry.find("name"));
        if (!name.has_value()) {
            return std::unexpected(missing("item name"));
        }
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> item =
            wire::common::Identifier::parse(*name);
        if (!item.has_value()) {
            return std::unexpected(LootTableParseError{"Malformed item name: " + *name});
        }
        Parsed<std::vector<LootFunction>> functions = parseFunctions(entry.find("functions"));
        if (!functions.has_value()) {
            return std::unexpected(functions.error());
        }
        LootEntry built;
        built.item = *item;
        built.weight = static_cast<std::int32_t>(asNumber(entry.find("weight")).value_or(1.0));
        built.quality = static_cast<std::int32_t>(asNumber(entry.find("quality")).value_or(0.0));
        built.conditions = std::move(*conditions);
        built.functions = std::move(*functions);
        return built;
    }
    if (*type == "minecraft:alternatives") {
        const std::vector<JsonValue>* children = asArray(entry.find("children"));
        if (children == nullptr) {
            return std::unexpected(missing("alternatives children"));
        }
        LootEntry built;
        built.conditions = std::move(*conditions);
        for (const JsonValue& element : *children) {
            const JsonObject* child = element.get<JsonObject>();
            if (child == nullptr) {
                return std::unexpected(missing("alternatives child"));
            }
            Parsed<LootEntry> parsed = parseEntry(*child);
            if (!parsed.has_value()) {
                return parsed;
            }
            built.alternatives.push_back(std::move(*parsed));
        }
        return built;
    }
    return std::unexpected(LootTableParseError{"Unsupported loot pool entry type: " + *type});
}

[[nodiscard]] Parsed<LootPool> parsePool(const JsonObject& pool) {
    const std::vector<JsonValue>* entries = asArray(pool.find("entries"));
    if (entries == nullptr) {
        return std::unexpected(missing("pool entries"));
    }
    LootPool built;
    for (const JsonValue& element : *entries) {
        const JsonObject* object = element.get<JsonObject>();
        if (object == nullptr) {
            return std::unexpected(missing("pool entry"));
        }
        Parsed<LootEntry> entry = parseEntry(*object);
        if (!entry.has_value()) {
            return std::unexpected(entry.error());
        }
        built.entries.push_back(std::move(*entry));
    }
    if (const JsonValue* rolls = pool.find("rolls"); rolls != nullptr) {
        Parsed<NumberProvider> parsed = parseNumberProvider(*rolls);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }
        built.rolls = std::move(*parsed);
    }
    if (const JsonValue* bonus = pool.find("bonus_rolls"); bonus != nullptr) {
        Parsed<NumberProvider> parsed = parseNumberProvider(*bonus);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }
        built.bonusRolls = std::move(*parsed);
    }
    Parsed<std::vector<LootCondition>> conditions = parseConditions(pool.find("conditions"));
    if (!conditions.has_value()) {
        return std::unexpected(conditions.error());
    }
    built.conditions = std::move(*conditions);
    Parsed<std::vector<LootFunction>> functions = parseFunctions(pool.find("functions"));
    if (!functions.has_value()) {
        return std::unexpected(functions.error());
    }
    built.functions = std::move(*functions);
    return built;
}

} // namespace

std::expected<LootTable, LootTableParseError> parse(std::string_view json) {
    const std::expected<JsonValue, wire::common::JsonError> root = wire::common::parseJson(json);
    if (!root.has_value()) {
        return std::unexpected(LootTableParseError{"Malformed loot table JSON"});
    }
    const JsonObject* object = root->get<JsonObject>();
    if (object == nullptr) {
        return std::unexpected(missing("loot table object"));
    }
    LootTable table;
    if (const std::vector<JsonValue>* pools = asArray(object->find("pools")); pools != nullptr) {
        for (const JsonValue& element : *pools) {
            const JsonObject* pool = element.get<JsonObject>();
            if (pool == nullptr) {
                return std::unexpected(missing("loot pool"));
            }
            Parsed<LootPool> parsed = parsePool(*pool);
            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }
            table.pools.push_back(std::move(*parsed));
        }
    }
    Parsed<std::vector<LootFunction>> functions = parseFunctions(object->find("functions"));
    if (!functions.has_value()) {
        return std::unexpected(functions.error());
    }
    table.functions = std::move(*functions);
    return table;
}

} // namespace basalt::server::world::loot::lootTableJson
