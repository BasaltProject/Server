#pragma once

#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/server/world/loot/LootContext.hpp>
#include <basalt/server/world/loot/LootTableJson.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::loot::lootTableJson::parts {

template <typename Value>
using Parsed = std::expected<Value, LootTableParseError>;

[[nodiscard]] LootTableParseError missing(std::string_view what);
[[nodiscard]] const wire::common::JsonObject* asObject(const wire::common::JsonValue* value);
[[nodiscard]] const std::vector<wire::common::JsonValue>* asArray(const wire::common::JsonValue* value);
[[nodiscard]] std::optional<std::string> asString(const wire::common::JsonValue* value);
[[nodiscard]] std::optional<double> asNumber(const wire::common::JsonValue* value);
[[nodiscard]] std::optional<bool> asBool(const wire::common::JsonValue* value);
[[nodiscard]] std::vector<std::string> idsOrTag(const wire::common::JsonValue* value);
[[nodiscard]] std::optional<std::string> typeOf(const wire::common::JsonObject& object);
[[nodiscard]] DoubleBounds doubleBounds(const wire::common::JsonValue* value);
[[nodiscard]] LocationPredicate parseLocation(const wire::common::JsonObject* predicate);
[[nodiscard]] ItemPredicate parseItemPredicate(const wire::common::JsonObject& object);
[[nodiscard]] EntityPredicate parseEntityPredicate(const wire::common::JsonObject& object);
[[nodiscard]] LevelBasedValue parseLevelBased(const wire::common::JsonValue* value);
/// parity: `StatePropertiesPredicate`, exact values and `{min, max}` ranges.
[[nodiscard]] StateMatcher parseStateMatcher(const wire::common::JsonObject* object);
[[nodiscard]] Parsed<LootFunction> parseComponentSetter(std::string_view type,
                                                        const wire::common::JsonObject& function);
[[nodiscard]] std::optional<EntityTarget> entityTargetOf(const wire::common::JsonValue* value);

class Reader {
public:
    explicit Reader(LootReferences* references) : references_(references) {}

    [[nodiscard]] Parsed<IntProvider> intProvider(const wire::common::JsonValue& value);
    [[nodiscard]] Parsed<FloatProvider> floatProvider(const wire::common::JsonValue& value);
    [[nodiscard]] Parsed<IntRange> intRange(const wire::common::JsonValue& value);
    [[nodiscard]] Parsed<FloatRange> floatRange(const wire::common::JsonValue& value);
    [[nodiscard]] Parsed<LootCondition> condition(const wire::common::JsonValue& value);
    [[nodiscard]] Parsed<std::vector<LootCondition>> optionalCondition(const wire::common::JsonValue* value);
    [[nodiscard]] Parsed<std::vector<LootCondition>> conditionSet(const wire::common::JsonValue& value);
    [[nodiscard]] Parsed<LootFunction> function(const wire::common::JsonValue& value);
    [[nodiscard]] Parsed<std::vector<LootFunction>> modifier(const wire::common::JsonValue* value);
    [[nodiscard]] Parsed<LootEntry> entry(const wire::common::JsonObject& object);
    [[nodiscard]] Parsed<LootPool> pool(const wire::common::JsonObject& object);
    [[nodiscard]] Parsed<LootTable> table(const wire::common::JsonObject& object);
    [[nodiscard]] std::optional<Parsed<LootFunction>> componentFunction(const std::string& type,
                                                                        const wire::common::JsonObject& object);
    [[nodiscard]] Parsed<SlotSource> slotSource(const wire::common::JsonValue& value);
    [[nodiscard]] Parsed<std::vector<std::string>> tagValues(std::string_view registry, const std::string& id);

private:
    LootReferences* references_;
    std::vector<std::string> resolving_;

    [[nodiscard]] Parsed<wire::common::JsonValue> named(std::string_view registry, const std::string& id);
    void finish(const std::string& key, LootReferences::Entry entry);
    [[nodiscard]] Parsed<IntProvider> typedInt(const wire::common::JsonObject& object);
    [[nodiscard]] Parsed<FloatProvider> typedFloat(const wire::common::JsonObject& object);
    [[nodiscard]] Parsed<FloatProvider> typedFloatOps(const std::string& type, const wire::common::JsonObject& object);
    [[nodiscard]] Parsed<LootCondition> typedCondition(const wire::common::JsonObject& object);
    [[nodiscard]] Parsed<LootCondition> typedWorldCondition(const std::string& type,
                                                            const wire::common::JsonObject& object);
    [[nodiscard]] Parsed<LootFunction> typedFunction(const wire::common::JsonObject& object);
    [[nodiscard]] Parsed<LootFunction> typedCountFunction(const std::string& type,
                                                          const wire::common::JsonObject& object);
};

} // namespace basalt::server::world::loot::lootTableJson::parts
