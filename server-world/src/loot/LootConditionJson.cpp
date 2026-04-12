#include "LootJsonParts.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::loot::lootTableJson::parts {
namespace {

using wire::common::JsonObject;
using wire::common::JsonValue;

[[nodiscard]] std::optional<LootCondition::Target> targetOf(const JsonObject& object) {
    const std::optional<std::string> entity = asString(object.find("entity"));
    if (entity == std::optional<std::string>{"attacker"}) {
        return LootCondition::Target::Attacker;
    }
    if (entity == std::optional<std::string>{"direct_attacker"}) {
        return LootCondition::Target::DirectAttacker;
    }
    if (entity == std::optional<std::string>{"attacking_player"}) {
        return LootCondition::Target::AttackingPlayer;
    }
    if (!entity.has_value() || *entity == "this") {
        return LootCondition::Target::This;
    }
    return std::nullopt;
}

} // namespace

std::optional<EntityTarget> entityTargetOf(const JsonValue* value) {
    static const std::map<std::string, EntityTarget, std::less<>> kTargets{
        {"this", EntityTarget::This},
        {"attacker", EntityTarget::Attacker},
        {"direct_attacker", EntityTarget::DirectAttacker},
        {"attacking_player", EntityTarget::AttackingPlayer},
        {"target_entity", EntityTarget::TargetEntity},
        {"interacting_entity", EntityTarget::InteractingEntity},
    };
    const std::optional<std::string> name = asString(value);
    const auto found = name.has_value() ? kTargets.find(*name) : kTargets.end();
    return found == kTargets.end() ? std::nullopt : std::optional{found->second};
}

Parsed<LootCondition> Reader::condition(const JsonValue& value) {
    if (const std::optional<std::string> id = asString(&value); id.has_value()) {
        const std::string key = "predicate/" + *id;
        if (references_ != nullptr) {
            if (std::optional<LootReferences::Entry> known = references_->cached(key); known.has_value()) {
                return std::get<LootCondition>(*known);
            }
        }
        Parsed<JsonValue> json = named("predicate", *id);
        if (!json.has_value()) {
            return std::unexpected(json.error());
        }
        Parsed<LootCondition> resolved = condition(*json);
        resolving_.pop_back();
        if (resolved.has_value()) {
            finish(key, *resolved);
        }
        return resolved;
    }
    const JsonObject* object = value.get<JsonObject>();
    if (object == nullptr) {
        return std::unexpected(missing("loot condition"));
    }
    return typedCondition(*object);
}

Parsed<std::vector<LootCondition>> Reader::optionalCondition(const JsonValue* value) {
    if (value == nullptr) {
        return std::vector<LootCondition>{};
    }
    Parsed<LootCondition> parsed = condition(*value);
    if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
    }
    return std::vector<LootCondition>{std::move(*parsed)};
}

Parsed<std::vector<LootCondition>> Reader::conditionSet(const JsonValue& value) {
    std::vector<LootCondition> terms;
    const std::vector<JsonValue>* list = value.get<std::vector<JsonValue>>();
    if (list == nullptr) {
        // parity: `LootItemCondition.LIST_CODEC` takes a `#tag` of the predicate registry too.
        if (const std::optional<std::string> tag = asString(&value); tag.has_value() && tag->starts_with('#')) {
            Parsed<std::vector<std::string>> members = tagValues("predicate", tag->substr(1));
            if (!members.has_value()) {
                return std::unexpected(members.error());
            }
            for (const std::string& member : *members) {
                Parsed<LootCondition> term = condition(JsonValue::of(member));
                if (!term.has_value()) {
                    return std::unexpected(term.error());
                }
                terms.push_back(std::move(*term));
            }
            return terms;
        }
        Parsed<LootCondition> single = condition(value);
        if (!single.has_value()) {
            return std::unexpected(single.error());
        }
        terms.push_back(std::move(*single));
        return terms;
    }
    for (const JsonValue& element : *list) {
        Parsed<LootCondition> term = condition(element);
        if (!term.has_value()) {
            return std::unexpected(term.error());
        }
        terms.push_back(std::move(*term));
    }
    return terms;
}

// parity: 26.3 dispatches conditions on `type`, where 26.2 used a `condition` key.
Parsed<LootCondition> Reader::typedCondition(const JsonObject& object) {
    const std::optional<std::string> type = typeOf(object);
    if (!type.has_value()) {
        return std::unexpected(missing("loot condition type"));
    }
    if (*type == "minecraft:random_chance") {
        const JsonValue* chance = object.find("chance");
        Parsed<FloatProvider> provider =
            chance == nullptr ? std::unexpected(missing("random chance")) : floatProvider(*chance);
        return provider.has_value() ? Parsed<LootCondition>{LootCondition::randomChance(std::move(*provider))}
                                    : std::unexpected(provider.error());
    }
    if (*type == "minecraft:survives_explosion") {
        return LootCondition::survivesExplosion();
    }
    if (*type == "minecraft:killed_by_player") {
        return LootCondition::killedByPlayer();
    }
    // parity: `MatchTool`, whose predicate is optional: without one any tool passes.
    if (*type == "minecraft:match_tool") {
        const JsonObject* predicate = asObject(object.find("predicate"));
        return predicate == nullptr ? LootCondition::matchTool()
                                    : LootCondition::matchTool(parseItemPredicate(*predicate));
    }
    // parity: `MatchBlock`, the `BlockPredicate` fields written inline: which blocks, and the state.
    if (*type == "minecraft:match_block") {
        return LootCondition::matchBlock(idsOrTag(object.find("blocks")),
                                         parseStateMatcher(asObject(object.find("state"))),
                                         object.find("nbt") != nullptr || object.find("components") != nullptr ||
                                             object.find("predicates") != nullptr);
    }
    // parity: `LootItemEntityPropertyCondition`: no predicate passes, an empty one asks only that the
    // entity be there, and the fishing hook is the one type-specific predicate modelled on its own.
    if (*type == "minecraft:entity_properties") {
        const std::optional<LootCondition::Target> target = targetOf(object);
        const JsonObject* predicate = asObject(object.find("predicate"));
        if (predicate == nullptr) {
            return LootCondition::always();
        }
        if (!target.has_value()) {
            return LootCondition::entityProperties();
        }
        if (predicate->empty()) {
            return LootCondition::entityExists(*target);
        }
        const JsonObject* hook = asObject(predicate->find("minecraft:type_specific/fishing_hook"));
        const JsonValue* openWater = hook == nullptr ? nullptr : hook->find("in_open_water");
        const bool* expected = openWater == nullptr ? nullptr : openWater->get<bool>();
        if (expected != nullptr && predicate->size() == 1 && hook->size() == 1) {
            return LootCondition::fishingHookOpenWater(*expected);
        }
        return LootCondition::entityMatches(*target, parseEntityPredicate(*predicate));
    }
    // parity: `DamageSourceCondition`, the damage's tags and the two entities behind it.
    if (*type == "minecraft:damage_source_properties") {
        const JsonObject* predicate = asObject(object.find("predicate"));
        if (predicate == nullptr) {
            return LootCondition::damageSource({}, std::nullopt, std::nullopt);
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
    if (*type == "minecraft:inverted") {
        const JsonValue* term = object.find("term");
        Parsed<LootCondition> inner = term == nullptr ? std::unexpected(missing("inverted term")) : condition(*term);
        return inner.has_value() ? Parsed<LootCondition>{LootCondition::inverted(std::move(*inner))} : inner;
    }
    if (*type == "minecraft:any_of" || *type == "minecraft:all_of") {
        const JsonValue* list = object.find("terms");
        Parsed<std::vector<LootCondition>> terms =
            list == nullptr ? std::unexpected(missing("condition terms")) : conditionSet(*list);
        if (!terms.has_value()) {
            return std::unexpected(terms.error());
        }
        return *type == "minecraft:any_of" ? LootCondition::anyOf(std::move(*terms))
                                           : LootCondition::allOf(std::move(*terms));
    }
    // parity: `LocationCheck`, a `LocationPredicate` at the origin moved by the three offsets.
    if (*type == "minecraft:location_check") {
        const auto offset = [&object](std::string_view key) {
            return static_cast<double>(static_cast<std::int32_t>(asNumber(object.find(key)).value_or(0.0)));
        };
        return LootCondition::locationCheck(parseLocation(asObject(object.find("predicate"))),
                                            {offset("offsetX"), offset("offsetY"), offset("offsetZ")});
    }
    if (*type == "minecraft:table_bonus") {
        const std::vector<JsonValue>* chances = asArray(object.find("chances"));
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
        return LootCondition::tableBonus(asString(object.find("enchantment")).value_or(""), std::move(values));
    }
    if (*type == "minecraft:random_chance_with_enchanted_bonus") {
        const std::optional<double> chance = asNumber(object.find("unenchanted_chance"));
        return LootCondition::randomChanceWithEnchantedBonus(static_cast<float>(chance.value_or(0.0)),
                                                             parseLevelBased(object.find("enchanted_chance")),
                                                             asString(object.find("enchantment")).value_or(""));
    }
    return typedWorldCondition(*type, object);
}

Parsed<LootCondition> Reader::typedWorldCondition(const std::string& type, const JsonObject& object) {
    // parity: `IntValueCheck` and `FloatValueCheck`, which split 26.2's `value_check` by kind.
    if (type == "minecraft:int_value_check" || type == "minecraft:float_value_check") {
        const JsonValue* value = object.find("value");
        const JsonValue* test = object.find("test");
        if (value == nullptr || test == nullptr) {
            return std::unexpected(missing(type + " value"));
        }
        if (type == "minecraft:int_value_check") {
            Parsed<IntProvider> provider = intProvider(*value);
            if (!provider.has_value()) {
                return std::unexpected(provider.error());
            }
            Parsed<IntRange> range = intRange(*test);
            return range.has_value()
                       ? Parsed<LootCondition>{LootCondition::intValueCheck(std::move(*provider), std::move(*range))}
                       : std::unexpected(range.error());
        }
        Parsed<FloatProvider> provider = floatProvider(*value);
        if (!provider.has_value()) {
            return std::unexpected(provider.error());
        }
        Parsed<FloatRange> range = floatRange(*test);
        return range.has_value()
                   ? Parsed<LootCondition>{LootCondition::floatValueCheck(std::move(*provider), std::move(*range))}
                   : std::unexpected(range.error());
    }
    if (type == "minecraft:enchantment_active_check") {
        const std::optional<bool> active = asBool(object.find("active"));
        return active.has_value() ? Parsed<LootCondition>{LootCondition::enchantmentActiveCheck(*active)}
                                  : std::unexpected(missing("enchantment_active_check active"));
    }
    if (type == "minecraft:weather_check") {
        return LootCondition::weatherCheck(asBool(object.find("raining")), asBool(object.find("thundering")));
    }
    if (type == "minecraft:time_check") {
        const std::optional<std::string> clock = asString(object.find("clock"));
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            clock.has_value() ? wire::common::Identifier::parse(*clock)
                              : std::unexpected(wire::common::IdentifierError::EmptyPart);
        const JsonValue* value = object.find("value");
        if (!id.has_value() || value == nullptr) {
            return std::unexpected(missing("time_check clock"));
        }
        Parsed<IntRange> range = intRange(*value);
        if (!range.has_value()) {
            return std::unexpected(range.error());
        }
        const std::optional<double> period = asNumber(object.find("period"));
        return LootCondition::timeCheck(
            *id, period.has_value() ? std::optional{static_cast<std::int64_t>(*period)} : std::nullopt,
            std::move(*range));
    }
    if (type == "minecraft:entity_scores") {
        const std::optional<EntityTarget> target = entityTargetOf(object.find("entity"));
        const JsonObject* scores = asObject(object.find("scores"));
        if (!target.has_value() || scores == nullptr) {
            return std::unexpected(missing("entity_scores"));
        }
        std::vector<std::pair<std::string, IntRange>> ranges;
        for (std::size_t index = 0; index < scores->names().size(); ++index) {
            Parsed<IntRange> range = intRange(scores->values()[index]);
            if (!range.has_value()) {
                return std::unexpected(range.error());
            }
            ranges.emplace_back(scores->names()[index], std::move(*range));
        }
        return LootCondition::entityScores(*target, std::move(ranges));
    }
    if (type == "minecraft:environment_attribute_check") {
        const std::optional<std::string> attribute = asString(object.find("attribute"));
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            attribute.has_value() ? wire::common::Identifier::parse(*attribute)
                                  : std::unexpected(wire::common::IdentifierError::EmptyPart);
        const JsonValue* value = object.find("value");
        const std::optional<double> number = asNumber(value);
        const std::optional<bool> flag =
            value == nullptr || value->get<bool>() == nullptr ? std::nullopt : std::optional{*value->get<bool>()};
        if (!id.has_value() || (!number.has_value() && !flag.has_value())) {
            return std::unexpected(LootTableParseError{"Unsupported environment attribute value"});
        }
        return LootCondition::environmentAttributeCheck(*id, flag.has_value() ? (*flag ? 1.0 : 0.0) : *number);
    }
    return std::unexpected(LootTableParseError{"Unsupported loot condition type: " + type});
}

} // namespace basalt::server::world::loot::lootTableJson::parts
