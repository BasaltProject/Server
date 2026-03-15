#include "LootJsonParts.hpp"

#include <basalt/server/world/loot/LootTableJson.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::loot::lootTableJson {
namespace {

using parts::asArray;
using parts::asBool;
using parts::asNumber;
using parts::asObject;
using parts::asString;
using parts::idsOrTag;
using parts::missing;
using parts::parseComponentSetter;
using parts::Parsed;
using parts::parseEntityPredicate;
using parts::parseItemPredicate;
using parts::parseLevelBased;
using wire::common::JsonObject;
using wire::common::JsonValue;

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
    if (*type == "minecraft:binomial") {
        const JsonValue* n = object.find("n");
        const JsonValue* p = object.find("p");
        if (n == nullptr || p == nullptr) {
            return std::unexpected(missing("binomial parameters"));
        }
        Parsed<NumberProvider> tries = parseNumberProvider(*n);
        if (!tries.has_value()) {
            return tries;
        }
        Parsed<NumberProvider> chance = parseNumberProvider(*p);
        if (!chance.has_value()) {
            return chance;
        }
        return NumberProvider::binomial(std::move(*tries), std::move(*chance));
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
        const JsonObject* predicate = asObject(condition.find("predicate"));
        return predicate == nullptr ? LootCondition::matchTool()
                                    : LootCondition::matchTool(parseItemPredicate(*predicate));
    }
    // parity: `LootItemEntityPropertyCondition` with an **empty** predicate is not a question about
    // the entity at all, it is a question about whether there is one: snow broken by a piston leaves
    // nothing and snow mined by hand leaves snowballs. A predicate with anything in it is a gap.
    if (*type == "minecraft:entity_properties") {
        const JsonObject* predicate = asObject(condition.find("predicate"));
        if (predicate == nullptr || predicate->empty()) {
            return LootCondition::thisEntityExists();
        }
        const JsonObject* hook = asObject(predicate->find("minecraft:type_specific/fishing_hook"));
        const JsonValue* openWater = hook == nullptr ? nullptr : hook->find("in_open_water");
        const bool* expected = openWater == nullptr ? nullptr : openWater->get<bool>();
        if (expected != nullptr && predicate->size() == 1 && hook->size() == 1) {
            return LootCondition::fishingHookOpenWater(*expected);
        }
        const std::optional<std::string> entity = asString(condition.find("entity"));
        const LootCondition::Target target =
            entity == std::optional<std::string>{"attacker"}          ? LootCondition::Target::Attacker
            : entity == std::optional<std::string>{"direct_attacker"} ? LootCondition::Target::DirectAttacker
                                                                      : LootCondition::Target::This;
        return LootCondition::entityMatches(target, parseEntityPredicate(*predicate));
    }
    // parity: the conditions a mob's own table carries, about what died and what killed it.
    // parity: `DamageSourceCondition`, the damage's tags and the two entities behind it.
    if (*type == "minecraft:damage_source_properties") {
        const JsonObject* predicate = asObject(condition.find("predicate"));
        if (predicate == nullptr) {
            return LootCondition::entityProperties();
        }
        std::vector<std::pair<std::string, bool>> tags;
        if (const std::vector<JsonValue>* list = asArray(predicate->find("tags")); list != nullptr) {
            for (const JsonValue& each : *list) {
                const JsonObject* tag = each.get<JsonObject>();
                const std::optional<std::string> id = tag == nullptr ? std::nullopt : asString(tag->find("id"));
                if (!id.has_value()) {
                    return std::unexpected(missing("damage tag id"));
                }
                tags.emplace_back(*id, asBool(tag->find("expected")).value_or(true));
            }
        }
        const auto entity = [predicate](std::string_view key) -> std::optional<EntityPredicate> {
            const JsonObject* inner = asObject(predicate->find(key));
            return inner == nullptr ? std::nullopt : std::optional{parseEntityPredicate(*inner)};
        };
        return LootCondition::damageSource(std::move(tags), entity("source_entity"), entity("direct_entity"));
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
    // parity: `LootItemBlockStatePropertyCondition`. Every value in a block table is an exact string
    // (checked against all 1113 of them), so the range form of a state predicate is a gap.
    if (*type == "minecraft:block_state_property") {
        const JsonObject* properties = asObject(condition.find("properties"));
        if (properties == nullptr) {
            return std::unexpected(missing("block state properties"));
        }
        std::map<std::string, std::string> expected;
        for (std::size_t index = 0; index < properties->names().size(); ++index) {
            const std::optional<std::string> value = asString(&properties->values()[index]);
            if (!value.has_value()) {
                return std::unexpected(missing("block state property value"));
            }
            expected.emplace(properties->names()[index], *value);
        }
        return LootCondition::blockStateProperty(std::move(expected));
    }
    // parity: `LocationCheck`, which this evaluator answers true to; see `LootCondition::test`.
    if (*type == "minecraft:location_check") {
        return LootCondition::locationCheck();
    }
    // parity: `BonusLevelTableCondition`, whose chance is the entry of its table for the enchantment
    // level of the tool. No tool reaches here, so the level is zero and the first entry is the one.
    if (*type == "minecraft:table_bonus") {
        const std::vector<JsonValue>* chances = asArray(condition.find("chances"));
        if (chances == nullptr || chances->empty()) {
            return std::unexpected(missing("table bonus chances"));
        }
        std::vector<float> values;
        for (const JsonValue& chance : *chances) {
            const std::optional<double> value = asNumber(&chance);
            if (!value.has_value()) {
                return std::unexpected(missing("table bonus chance"));
            }
            values.push_back(static_cast<float>(*value));
        }
        return LootCondition::tableBonus(asString(condition.find("enchantment")).value_or(""), std::move(values));
    }
    if (*type == "minecraft:random_chance_with_enchanted_bonus") {
        const std::optional<double> chance = asNumber(condition.find("unenchanted_chance"));
        return LootCondition::randomChanceWithEnchantedBonus(static_cast<float>(chance.value_or(0.0)),
                                                             parseLevelBased(condition.find("enchanted_chance")),
                                                             asString(condition.find("enchantment")).value_or(""));
    }
    return std::unexpected(LootTableParseError{"Unsupported loot condition type: " + *type});
}

// Functions dispatch on `function`, not `type`: the same explicit key conditions use.
Parsed<LootFunction> parseFunction(const JsonObject& function) {
    const std::optional<std::string> type = asString(function.find("function"));
    if (!type.has_value()) {
        return std::unexpected(missing("loot function type"));
    }
    // parity: every loot function is a `LootItemConditionalFunction`, so any of them can carry the
    // conditions that decide whether it runs at all.
    Parsed<std::vector<LootCondition>> gates = parseConditions(function.find("conditions"));
    if (!gates.has_value()) {
        return std::unexpected(gates.error());
    }
    const auto gated = [&gates](Parsed<LootFunction> parsed) {
        if (parsed.has_value()) {
            parsed->conditions = std::move(*gates);
        }
        return parsed;
    };
    if (*type == "minecraft:set_count") {
        const JsonValue* count = function.find("count");
        if (count == nullptr) {
            return std::unexpected(missing("set_count count"));
        }
        Parsed<NumberProvider> provider = parseNumberProvider(*count);
        if (!provider.has_value()) {
            return std::unexpected(provider.error());
        }
        return gated(LootFunction::setCount(std::move(*provider), asBool(function.find("add")).value_or(false)));
    }
    // parity: `furnace_smelt`, the cooked drop a mob killed by fire leaves, and the two that only
    // set a component: the item itself is right without them, so they are read and left alone.
    if (*type == "minecraft:furnace_smelt") {
        return gated(LootFunction::smelt());
    }
    if (*type == "minecraft:set_potion" || *type == "minecraft:set_ominous_bottle_amplifier") {
        return gated(parseComponentSetter(function));
    }
    if (*type == "minecraft:enchant_randomly") {
        return gated(LootFunction::enchantRandomly(idsOrTag(function.find("options")),
                                                   asBool(function.find("only_compatible")).value_or(true)));
    }
    if (*type == "minecraft:enchant_with_levels") {
        const JsonValue* levels = function.find("levels");
        if (levels == nullptr) {
            return std::unexpected(missing("enchant_with_levels levels"));
        }
        Parsed<NumberProvider> provider = parseNumberProvider(*levels);
        if (!provider.has_value()) {
            return std::unexpected(provider.error());
        }
        return gated(LootFunction::enchantWithLevels(std::move(*provider), idsOrTag(function.find("options"))));
    }
    if (*type == "minecraft:set_enchantments" || *type == "minecraft:set_stew_effect") {
        std::vector<std::pair<std::string, NumberProvider>> entries;
        const bool stew = *type == "minecraft:set_stew_effect";
        if (stew) {
            if (const std::vector<JsonValue>* effects = asArray(function.find("effects")); effects != nullptr) {
                for (const JsonValue& each : *effects) {
                    const JsonObject* effect = each.get<JsonObject>();
                    const std::optional<std::string> id =
                        effect == nullptr ? std::nullopt : asString(effect->find("type"));
                    const JsonValue* duration = effect == nullptr ? nullptr : effect->find("duration");
                    if (!id.has_value() || duration == nullptr) {
                        return std::unexpected(missing("stew effect"));
                    }
                    Parsed<NumberProvider> provider = parseNumberProvider(*duration);
                    if (!provider.has_value()) {
                        return std::unexpected(provider.error());
                    }
                    entries.emplace_back(*id, std::move(*provider));
                }
            }
            return gated(LootFunction::setStewEffect(std::move(entries)));
        }
        if (const JsonObject* levels = asObject(function.find("enchantments")); levels != nullptr) {
            for (std::size_t index = 0; index < levels->names().size(); ++index) {
                Parsed<NumberProvider> provider = parseNumberProvider(levels->values()[index]);
                if (!provider.has_value()) {
                    return std::unexpected(provider.error());
                }
                entries.emplace_back(levels->names()[index], std::move(*provider));
            }
        }
        return gated(LootFunction::setEnchantments(std::move(entries), asBool(function.find("add")).value_or(false)));
    }
    if (*type == "minecraft:set_damage") {
        const JsonValue* damage = function.find("damage");
        if (damage == nullptr) {
            return std::unexpected(missing("set_damage damage"));
        }
        Parsed<NumberProvider> provider = parseNumberProvider(*damage);
        if (!provider.has_value()) {
            return std::unexpected(provider.error());
        }
        return gated(LootFunction::setDamage(std::move(*provider), asBool(function.find("add")).value_or(false)));
    }
    if (*type == "minecraft:set_components" || *type == "minecraft:set_name") {
        return gated(parseComponentSetter(function));
    }
    if (*type == "minecraft:set_instrument") {
        return gated(LootFunction::setInstrument(asString(function.find("options")).value_or("")));
    }
    // parity: `ExplorationMapFunction`, which hands the plain map back when it finds no structure.
    // This build searches for none, so the map stays blank.
    if (*type == "minecraft:exploration_map") {
        return gated(LootFunction::copyFromBlock());
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
        return gated(LootFunction::enchantedCountIncrease(
            asString(function.find("enchantment")).value_or(""), std::move(*provider),
            limit.has_value() ? std::optional<std::int32_t>{static_cast<std::int32_t>(*limit)} : std::nullopt));
    }
    // parity: `ApplyExplosionDecay`, which is what leaves a crater with only some of what it broke.
    // Refusing it left every table that carries it dropping nothing at all.
    if (*type == "minecraft:explosion_decay") {
        return gated(LootFunction::explosionDecay());
    }
    // parity: `ApplyBonusCount`, which needs the level of the enchantment on the tool that broke the
    // block. There is none here, and at level zero all three of its formulas leave the count alone.
    if (*type == "minecraft:apply_bonus") {
        const std::optional<std::string> formula = asString(function.find("formula"));
        const JsonObject* parameters = asObject(function.find("parameters"));
        const auto parameter = [parameters](std::string_view name, double fallback) {
            return parameters == nullptr ? fallback : asNumber(parameters->find(name)).value_or(fallback);
        };
        LootFunction::BonusFormula kind = LootFunction::BonusFormula::OreDrops;
        std::int32_t number = 0;
        if (formula == std::optional<std::string>{"minecraft:uniform_bonus_count"}) {
            kind = LootFunction::BonusFormula::UniformBonusCount;
            number = static_cast<std::int32_t>(parameter("bonusMultiplier", 1.0));
        } else if (formula == std::optional<std::string>{"minecraft:binomial_with_bonus_count"}) {
            kind = LootFunction::BonusFormula::BinomialWithBonusCount;
            number = static_cast<std::int32_t>(parameter("extra", 0.0));
        } else if (formula != std::optional<std::string>{"minecraft:ore_drops"}) {
            return std::unexpected(LootTableParseError{"Unsupported bonus formula"});
        }
        return gated(LootFunction::applyBonus(asString(function.find("enchantment")).value_or(""), kind, number,
                                              static_cast<float>(parameter("probability", 0.0))));
    }
    // parity: `LimitCount`, `IntRange` with either end optional.
    if (*type == "minecraft:limit_count") {
        const JsonValue* limit = function.find("limit");
        if (const std::optional<double> exact = asNumber(limit); exact.has_value()) {
            const auto bound = static_cast<std::int32_t>(*exact);
            return gated(LootFunction::limitCount(bound, bound));
        }
        const JsonObject* range = asObject(limit);
        if (range == nullptr) {
            return std::unexpected(missing("limit count range"));
        }
        const std::optional<double> least = asNumber(range->find("min"));
        const std::optional<double> most = asNumber(range->find("max"));
        return gated(LootFunction::limitCount(
            least.has_value() ? std::optional{static_cast<std::int32_t>(*least)} : std::nullopt,
            most.has_value() ? std::optional{static_cast<std::int32_t>(*most)} : std::nullopt));
    }
    // parity: the four that carry something off the block entity onto the stack. What they carry is
    // put on by the caller that knows the block, so the drop itself is right without them, and
    // refusing the whole table left the block dropping nothing.
    if (*type == "minecraft:copy_components" || *type == "minecraft:copy_custom_data" ||
        *type == "minecraft:copy_state" || *type == "minecraft:copy_name") {
        return gated(LootFunction::copyFromBlock());
    }
    return std::unexpected(LootTableParseError{"Unsupported loot function type: " + *type});
}

[[nodiscard]] Parsed<LootTable> parseTable(const JsonObject& object);

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
    // parity: `DynamicLoot`, whose stacks the block itself provides under this name.
    if (*type == "minecraft:dynamic") {
        const std::optional<std::string> name = asString(entry.find("name"));
        if (!name.has_value()) {
            return std::unexpected(missing("dynamic drop name"));
        }
        Parsed<std::vector<LootFunction>> functions = parseFunctions(entry.find("functions"));
        if (!functions.has_value()) {
            return std::unexpected(functions.error());
        }
        LootEntry built;
        built.dynamicName = *name;
        built.weight = static_cast<std::int32_t>(asNumber(entry.find("weight")).value_or(1.0));
        built.quality = static_cast<std::int32_t>(asNumber(entry.find("quality")).value_or(0.0));
        built.conditions = std::move(*conditions);
        built.functions = std::move(*functions);
        return built;
    }
    if (*type == "minecraft:loot_table") {
        LootEntry built;
        if (const JsonObject* inlined = asObject(entry.find("value")); inlined != nullptr) {
            Parsed<LootTable> table = parseTable(*inlined);
            if (!table.has_value()) {
                return std::unexpected(table.error());
            }
            built.inlineTable = std::make_shared<const LootTable>(std::move(*table));
        } else {
            const std::optional<std::string> value = asString(entry.find("value"));
            const std::expected<wire::common::Identifier, wire::common::IdentifierError> table =
                value.has_value() ? wire::common::Identifier::parse(*value)
                                  : std::unexpected(wire::common::IdentifierError::EmptyPart);
            if (!table.has_value()) {
                return std::unexpected(missing("loot table name"));
            }
            built.table = *table;
        }
        Parsed<std::vector<LootFunction>> functions = parseFunctions(entry.find("functions"));
        if (!functions.has_value()) {
            return std::unexpected(functions.error());
        }
        built.weight = static_cast<std::int32_t>(asNumber(entry.find("weight")).value_or(1.0));
        built.quality = static_cast<std::int32_t>(asNumber(entry.find("quality")).value_or(0.0));
        built.conditions = std::move(*conditions);
        built.functions = std::move(*functions);
        return built;
    }
    // parity: `EmptyLootItem` and `TagEntry`, both leaves with a weight.
    if (*type == "minecraft:empty" || *type == "minecraft:tag") {
        Parsed<std::vector<LootFunction>> functions = parseFunctions(entry.find("functions"));
        if (!functions.has_value()) {
            return std::unexpected(functions.error());
        }
        LootEntry built;
        if (*type == "minecraft:tag") {
            const std::optional<std::string> name = asString(entry.find("name"));
            const std::expected<wire::common::Identifier, wire::common::IdentifierError> tag =
                name.has_value() ? wire::common::Identifier::parse(*name)
                                 : std::unexpected(wire::common::IdentifierError::EmptyPart);
            if (!tag.has_value()) {
                return std::unexpected(missing("tag entry name"));
            }
            built.tag = *tag;
            built.expand = asBool(entry.find("expand")).value_or(false);
        } else {
            built.empty = true;
        }
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

Parsed<LootTable> parseTable(const JsonObject& object) {
    LootTable table;
    if (const std::vector<JsonValue>* pools = asArray(object.find("pools")); pools != nullptr) {
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
    // parity: `LootTable.randomSequence`, the name of the stream this table rolls on. Every table
    // the game ships carries one; a datapack's may not, and then it rolls on the level's own random.
    if (const std::optional<std::string> sequence = asString(object.find("random_sequence")); sequence.has_value()) {
        if (const std::expected<wire::common::Identifier, wire::common::IdentifierError> name =
                wire::common::Identifier::parse(*sequence);
            name.has_value()) {
            table.randomSequence = *name;
        }
    }
    Parsed<std::vector<LootFunction>> functions = parseFunctions(object.find("functions"));
    if (!functions.has_value()) {
        return std::unexpected(functions.error());
    }
    table.functions = std::move(*functions);
    return table;
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
    return parseTable(*object);
}

} // namespace basalt::server::world::loot::lootTableJson
