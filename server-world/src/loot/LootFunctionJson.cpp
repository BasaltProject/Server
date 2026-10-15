#include "LootJsonParts.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::loot::lootTableJson::parts {
namespace {

using wire::common::JsonObject;
using wire::common::JsonValue;

[[nodiscard]] bool copiesOnly(std::string_view type) {
    return type == "minecraft:copy_components" || type == "minecraft:copy_custom_data" ||
           type == "minecraft:copy_state" || type == "minecraft:copy_name";
}

} // namespace

Parsed<LootFunction> Reader::function(const JsonValue& value) {
    Parsed<std::vector<LootFunction>> functions = modifier(&value);
    if (!functions.has_value()) {
        return std::unexpected(functions.error());
    }
    // parity: a list is `SequenceFunction`'s inline form, a one-element list included.
    if (functions->size() == 1 && value.get<std::vector<JsonValue>>() == nullptr) {
        return std::move(functions->front());
    }
    return LootFunction::sequence(std::move(*functions));
}

Parsed<std::vector<LootFunction>> Reader::modifier(const JsonValue* value) {
    std::vector<LootFunction> functions;
    if (value == nullptr) {
        return functions;
    }
    if (const std::optional<std::string> id = asString(value); id.has_value()) {
        const std::string key = "item_modifier/" + *id;
        if (references_ != nullptr) {
            if (std::optional<LootReferences::Entry> known = references_->cached(key); known.has_value()) {
                return std::get<std::vector<LootFunction>>(*known);
            }
        }
        Parsed<JsonValue> json = named("item_modifier", *id);
        if (!json.has_value()) {
            return std::unexpected(json.error());
        }
        Parsed<std::vector<LootFunction>> resolved = modifier(&*json);
        resolving_.pop_back();
        if (resolved.has_value()) {
            finish(key, *resolved);
        }
        return resolved;
    }
    if (const JsonObject* object = value->get<JsonObject>(); object != nullptr) {
        Parsed<LootFunction> single = typedFunction(*object);
        if (!single.has_value()) {
            return std::unexpected(single.error());
        }
        functions.push_back(std::move(*single));
        return functions;
    }
    const std::vector<JsonValue>* list = value->get<std::vector<JsonValue>>();
    if (list == nullptr) {
        return std::unexpected(missing("item modifier"));
    }
    // parity: `LootItemFunctions.LIST_CODEC`, each element a named modifier or a typed one.
    for (const JsonValue& element : *list) {
        if (element.get<std::vector<JsonValue>>() != nullptr) {
            return std::unexpected(missing("item modifier list element"));
        }
        Parsed<LootFunction> one = function(element);
        if (!one.has_value()) {
            return std::unexpected(one.error());
        }
        functions.push_back(std::move(*one));
    }
    return functions;
}

// parity: 26.3 dispatches functions on `type`, and each carries one optional `condition`.
Parsed<LootFunction> Reader::typedFunction(const JsonObject& object) {
    const std::optional<std::string> type = typeOf(object);
    if (!type.has_value()) {
        return std::unexpected(missing("loot function type"));
    }
    Parsed<std::vector<LootCondition>> gates = optionalCondition(object.find("condition"));
    if (!gates.has_value()) {
        return std::unexpected(gates.error());
    }
    Parsed<LootFunction> parsed = typedCountFunction(*type, object);
    if (parsed.has_value()) {
        parsed->conditions = std::move(*gates);
    }
    return parsed;
}

Parsed<LootFunction> Reader::typedCountFunction(const std::string& type, const JsonObject& object) {
    const auto intField = [&](std::string_view key) -> Parsed<IntProvider> {
        const JsonValue* value = object.find(key);
        return value == nullptr ? std::unexpected(missing(type + " " + std::string{key})) : intProvider(*value);
    };
    const auto floatField = [&](std::string_view key) -> Parsed<FloatProvider> {
        const JsonValue* value = object.find(key);
        return value == nullptr ? std::unexpected(missing(type + " " + std::string{key})) : floatProvider(*value);
    };
    const bool add = asBool(object.find("add")).value_or(false);
    if (type == "minecraft:set_count") {
        Parsed<IntProvider> count = intField("count");
        return count.has_value() ? Parsed<LootFunction>{LootFunction::setCount(std::move(*count), add)}
                                 : std::unexpected(count.error());
    }
    if (type == "minecraft:set_damage") {
        Parsed<FloatProvider> damage = floatField("damage");
        return damage.has_value() ? Parsed<LootFunction>{LootFunction::setDamage(std::move(*damage), add)}
                                  : std::unexpected(damage.error());
    }
    if (type == "minecraft:enchanted_count_increase") {
        Parsed<FloatProvider> count = floatField("count");
        if (!count.has_value()) {
            return std::unexpected(count.error());
        }
        const std::optional<double> limit = asNumber(object.find("limit"));
        return LootFunction::enchantedCountIncrease(
            asString(object.find("enchantment")).value_or(""), std::move(*count),
            limit.has_value() ? std::optional{static_cast<std::int32_t>(*limit)} : std::nullopt);
    }
    if (type == "minecraft:enchant_with_levels") {
        Parsed<IntProvider> levels = intField("levels");
        if (!levels.has_value()) {
            return std::unexpected(levels.error());
        }
        LootFunction built = LootFunction::enchantWithLevels(std::move(*levels), idsOrTag(object.find("options")));
        built.includeAdditionalCost(asBool(object.find("include_additional_cost_component")).value_or(false));
        return built;
    }
    if (type == "minecraft:set_ominous_bottle_amplifier") {
        Parsed<IntProvider> amplifier = intField("amplifier");
        return amplifier.has_value()
                   ? Parsed<LootFunction>{LootFunction::setOminousBottleAmplifier(std::move(*amplifier))}
                   : std::unexpected(amplifier.error());
    }
    if (type == "minecraft:set_random_dyes") {
        Parsed<IntProvider> dyes = intField("number_of_dyes");
        return dyes.has_value() ? Parsed<LootFunction>{LootFunction::setRandomDyes(std::move(*dyes))}
                                : std::unexpected(dyes.error());
    }
    if (type == "minecraft:set_enchantments" || type == "minecraft:set_stew_effect") {
        std::vector<std::pair<std::string, IntProvider>> entries;
        if (type == "minecraft:set_stew_effect") {
            if (const std::vector<JsonValue>* effects = asArray(object.find("effects")); effects != nullptr) {
                for (const JsonValue& each : *effects) {
                    const JsonObject* effect = each.get<JsonObject>();
                    const std::optional<std::string> id =
                        effect == nullptr ? std::nullopt : asString(effect->find("type"));
                    const JsonValue* duration = effect == nullptr ? nullptr : effect->find("duration");
                    if (!id.has_value() || duration == nullptr) {
                        return std::unexpected(missing("stew effect"));
                    }
                    Parsed<IntProvider> provider = intProvider(*duration);
                    if (!provider.has_value()) {
                        return std::unexpected(provider.error());
                    }
                    entries.emplace_back(*id, std::move(*provider));
                }
            }
            return LootFunction::setStewEffect(std::move(entries));
        }
        if (const JsonObject* levels = asObject(object.find("enchantments")); levels != nullptr) {
            for (std::size_t index = 0; index < levels->names().size(); ++index) {
                Parsed<IntProvider> provider = intProvider(levels->values()[index]);
                if (!provider.has_value()) {
                    return std::unexpected(provider.error());
                }
                entries.emplace_back(levels->names()[index], std::move(*provider));
            }
        }
        return LootFunction::setEnchantments(std::move(entries), add);
    }
    // parity: `LimitCount` over an `IntLimit`.
    if (type == "minecraft:limit_count") {
        const JsonObject* limit = asObject(object.find("limit"));
        if (limit == nullptr) {
            return std::unexpected(missing("limit_count limit"));
        }
        std::optional<IntProvider> least;
        std::optional<IntProvider> most;
        for (const std::string_view key : {std::string_view{"min"}, std::string_view{"max"}}) {
            if (const JsonValue* bound = limit->find(key); bound != nullptr) {
                Parsed<IntProvider> parsed = intProvider(*bound);
                if (!parsed.has_value()) {
                    return std::unexpected(parsed.error());
                }
                (key == "min" ? least : most) = std::move(*parsed);
            }
        }
        return LootFunction::limitCount(std::move(least), std::move(most));
    }
    if (type == "minecraft:furnace_smelt") {
        return LootFunction::smelt(asBool(object.find("use_input_count")).value_or(true));
    }
    if (type == "minecraft:set_potion" || type == "minecraft:set_components" || type == "minecraft:set_name") {
        return parseComponentSetter(type, object);
    }
    if (type == "minecraft:enchant_randomly") {
        LootFunction built = LootFunction::enchantRandomly(idsOrTag(object.find("options")),
                                                           asBool(object.find("only_compatible")).value_or(true));
        built.includeAdditionalCost(asBool(object.find("include_additional_cost_component")).value_or(false));
        return built;
    }
    if (type == "minecraft:set_instrument") {
        return LootFunction::setInstrument(idsOrTag(object.find("options")));
    }
    // parity: without `options`, any potion of the registry.
    if (type == "minecraft:set_random_potion") {
        const JsonValue* options = object.find("options");
        return LootFunction::setRandomPotion(idsOrTag(options), options == nullptr);
    }
    if (type == "minecraft:explosion_decay") {
        return LootFunction::explosionDecay();
    }
    if (type == "minecraft:discard") {
        return LootFunction::discard();
    }
    if (type == "minecraft:sequence") {
        const JsonValue* functions = object.find("functions");
        if (functions == nullptr || functions->get<std::vector<JsonValue>>() == nullptr) {
            return std::unexpected(missing("sequence functions"));
        }
        Parsed<std::vector<LootFunction>> parsed = modifier(functions);
        return parsed.has_value() ? Parsed<LootFunction>{LootFunction::sequence(std::move(*parsed))}
                                  : std::unexpected(parsed.error());
    }
    if (type == "minecraft:filtered") {
        const JsonObject* filter = asObject(object.find("item_filter"));
        if (filter == nullptr) {
            return std::unexpected(missing("filtered item_filter"));
        }
        std::array<std::optional<LootFunction>, 2> branches;
        constexpr std::array<std::string_view, 2> kKeys{"on_pass", "on_fail"};
        for (std::size_t index = 0; index < kKeys.size(); ++index) {
            if (const JsonValue* branch = object.find(kKeys[index]); branch != nullptr) {
                Parsed<LootFunction> parsed = function(*branch);
                if (!parsed.has_value()) {
                    return parsed;
                }
                branches[index] = std::move(*parsed);
            }
        }
        return LootFunction::filtered(parseItemPredicate(*filter), std::move(branches[0]), std::move(branches[1]));
    }
    // parity: `ApplyBonusCount`, three formulas by the level of the enchantment on the tool.
    if (type == "minecraft:apply_bonus") {
        const std::optional<std::string> formula = asString(object.find("formula"));
        const JsonObject* parameters = asObject(object.find("parameters"));
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
        return LootFunction::applyBonus(asString(object.find("enchantment")).value_or(""), kind, number,
                                        static_cast<float>(parameter("probability", 0.0)));
    }
    if (type == "minecraft:exploration_map") {
        ExplorationMapRequest request;
        if (const JsonValue* destination = object.find("destination"); destination != nullptr) {
            if (const std::optional<std::string> one = asString(destination); one.has_value()) {
                request.destination.push_back(*one);
            } else if (const auto* list = destination->get<std::vector<JsonValue>>(); list != nullptr) {
                for (const JsonValue& element : *list) {
                    if (const std::optional<std::string> id = asString(&element); id.has_value()) {
                        request.destination.push_back(*id);
                    }
                }
            }
        }
        request.decoration = asString(object.find("decoration")).value_or(request.decoration);
        request.zoom = static_cast<std::int8_t>(asNumber(object.find("zoom")).value_or(request.zoom));
        request.searchRadius =
            static_cast<std::int32_t>(asNumber(object.find("search_radius")).value_or(request.searchRadius));
        if (const JsonValue* skip = object.find("skip_existing_chunks");
            skip != nullptr && skip->get<bool>() != nullptr) {
            request.skipExistingChunks = *skip->get<bool>();
        }
        return LootFunction::explorationMap(std::move(request));
    }
    if (copiesOnly(type)) {
        return LootFunction::copyFromBlock();
    }
    if (std::optional<Parsed<LootFunction>> edit = componentFunction(type, object); edit.has_value()) {
        return std::move(*edit);
    }
    return std::unexpected(LootTableParseError{"Unsupported loot function type: " + type});
}

} // namespace basalt::server::world::loot::lootTableJson::parts
