#include "LootJsonParts.hpp"

#include <basalt/server/world/loot/LootTableJson.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::loot {

std::optional<std::string> LootReferences::read(std::string_view registry, const wire::common::Identifier& id) const {
    return read_ ? read_(registry, id) : std::nullopt;
}

std::optional<LootReferences::Entry> LootReferences::cached(const std::string& key) const {
    const std::lock_guard<std::mutex> guard(mutex_);
    const auto found = cache_.find(key);
    return found == cache_.end() ? std::nullopt : std::optional{found->second};
}

void LootReferences::remember(const std::string& key, Entry entry) {
    const std::lock_guard<std::mutex> guard(mutex_);
    cache_.try_emplace(key, std::move(entry));
}

void LootReferences::replace(ReadEntry read) {
    const std::lock_guard<std::mutex> guard(mutex_);
    read_ = std::move(read);
    cache_.clear();
}

namespace lootTableJson {
namespace parts {
namespace {

using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;

[[nodiscard]] Parsed<Identifier> identifierOf(const std::optional<std::string>& text, std::string_view what) {
    const std::expected<Identifier, wire::common::IdentifierError> id =
        text.has_value() ? Identifier::parse(*text) : std::unexpected(wire::common::IdentifierError::EmptyPart);
    return id.has_value() ? Parsed<Identifier>{*id} : std::unexpected(missing(what));
}

/// The composite entry types and how they combine their children.
[[nodiscard]] std::optional<LootEntry::Composite> compositeOf(std::string_view type) {
    if (type == "minecraft:alternatives") {
        return LootEntry::Composite::Alternatives;
    }
    if (type == "minecraft:sequence") {
        return LootEntry::Composite::Sequence;
    }
    if (type == "minecraft:group") {
        return LootEntry::Composite::Group;
    }
    return std::nullopt;
}

} // namespace

Parsed<JsonValue> Reader::named(std::string_view registry, const std::string& id) {
    const std::string key = std::string{registry} + "/" + id;
    const Parsed<Identifier> parsedId = identifierOf(id, "registry reference");
    if (!parsedId.has_value()) {
        return std::unexpected(parsedId.error());
    }
    if (references_ == nullptr) {
        return std::unexpected(LootTableParseError{"Unresolved " + std::string{registry} + " reference: " + id});
    }
    if (std::ranges::find(resolving_, key) != resolving_.end()) {
        return std::unexpected(LootTableParseError{"Cyclic " + std::string{registry} + " reference: " + id});
    }
    const std::optional<std::string> text = references_->read(registry, *parsedId);
    if (!text.has_value()) {
        return std::unexpected(LootTableParseError{"Unknown " + std::string{registry} + ": " + id});
    }
    std::expected<JsonValue, wire::common::JsonError> json = wire::common::parseJson(*text);
    if (!json.has_value()) {
        return std::unexpected(LootTableParseError{"Malformed " + std::string{registry} + " JSON: " + id});
    }
    resolving_.push_back(key);
    return std::move(*json);
}

void Reader::finish(const std::string& key, LootReferences::Entry entry) {
    if (references_ != nullptr) {
        references_->remember(key, std::move(entry));
    }
}

Parsed<LootEntry> Reader::entry(const JsonObject& object) {
    const std::optional<std::string> type = typeOf(object);
    if (!type.has_value()) {
        return std::unexpected(missing("loot entry type"));
    }
    LootEntry built;
    Parsed<std::vector<LootCondition>> conditions = optionalCondition(object.find("condition"));
    if (!conditions.has_value()) {
        return std::unexpected(conditions.error());
    }
    built.conditions = std::move(*conditions);
    Parsed<std::vector<LootFunction>> functions = modifier(object.find("modifier"));
    if (!functions.has_value()) {
        return std::unexpected(functions.error());
    }
    built.functions = std::move(*functions);
    if (const std::optional<LootEntry::Composite> composite = compositeOf(*type); composite.has_value()) {
        built.composite = *composite;
        if (const std::vector<JsonValue>* children = asArray(object.find("children")); children != nullptr) {
            for (const JsonValue& element : *children) {
                const JsonObject* child = element.get<JsonObject>();
                if (child == nullptr) {
                    return std::unexpected(missing("composite child"));
                }
                Parsed<LootEntry> parsed = entry(*child);
                if (!parsed.has_value()) {
                    return parsed;
                }
                built.children.push_back(std::move(*parsed));
            }
        }
        return built;
    }
    built.weight = static_cast<std::int32_t>(asNumber(object.find("weight")).value_or(1.0));
    built.quality = static_cast<std::int32_t>(asNumber(object.find("quality")).value_or(0.0));
    built.expand = asBool(object.find("expand")).value_or(false);
    if (*type == "minecraft:item") {
        Parsed<Identifier> item = identifierOf(asString(object.find("name")), "item name");
        if (!item.has_value()) {
            return std::unexpected(item.error());
        }
        built.item = *item;
        return built;
    }
    // parity: `DynamicLoot`, whose stacks the block itself provides under this name.
    if (*type == "minecraft:dynamic") {
        const std::optional<std::string> name = asString(object.find("name"));
        if (!name.has_value()) {
            return std::unexpected(missing("dynamic drop name"));
        }
        built.dynamicName = *name;
        return built;
    }
    if (*type == "minecraft:empty") {
        built.empty = true;
        return built;
    }
    // parity: 26.3 `TagEntry` reads an item holder set from `items`, a `#tag` or ids written out.
    if (*type == "minecraft:tag") {
        const std::vector<std::string> items = idsOrTag(object.find("items"));
        if (items.empty()) {
            return std::unexpected(missing("tag entry items"));
        }
        if (items.size() == 1 && items.front().starts_with('#')) {
            Parsed<Identifier> tag = identifierOf(items.front().substr(1), "tag entry items");
            if (!tag.has_value()) {
                return std::unexpected(tag.error());
            }
            built.tag = *tag;
            return built;
        }
        for (const std::string& item : items) {
            Parsed<Identifier> id = identifierOf(item, "tag entry item");
            if (!id.has_value()) {
                return std::unexpected(id.error());
            }
            built.tagItems.push_back(*id);
        }
        return built;
    }
    // parity: 26.3 `NestedLootTable` reads a loot table holder set: ids, tables written in place, or
    // a `#tag`.
    if (*type == "minecraft:loot_table") {
        const JsonValue* value = object.find("value");
        if (value == nullptr) {
            return std::unexpected(missing("loot table value"));
        }
        std::vector<const JsonValue*> holders;
        if (const std::vector<JsonValue>* list = value->get<std::vector<JsonValue>>(); list != nullptr) {
            for (const JsonValue& element : *list) {
                holders.push_back(&element);
            }
        } else {
            holders.push_back(value);
        }
        for (const JsonValue* holder : holders) {
            if (const JsonObject* inlined = holder->get<JsonObject>(); inlined != nullptr) {
                Parsed<LootTable> nested = table(*inlined);
                if (!nested.has_value()) {
                    return std::unexpected(nested.error());
                }
                built.tables.push_back(NestedTable{.inlined = std::make_shared<const LootTable>(std::move(*nested))});
                continue;
            }
            const std::optional<std::string> name = asString(holder);
            const bool tag = name.has_value() && name->starts_with('#') && holders.size() == 1;
            Parsed<Identifier> id = identifierOf(tag ? std::optional{name->substr(1)} : name, "loot table name");
            if (!id.has_value()) {
                return std::unexpected(id.error());
            }
            if (tag) {
                built.tableTag = *id;
            } else {
                built.tables.push_back(NestedTable{.id = *id});
            }
        }
        return built;
    }
    return std::unexpected(LootTableParseError{"Unsupported loot pool entry type: " + *type});
}

Parsed<LootPool> Reader::pool(const JsonObject& object) {
    const std::vector<JsonValue>* entries = asArray(object.find("entries"));
    if (entries == nullptr) {
        return std::unexpected(missing("pool entries"));
    }
    LootPool built;
    for (const JsonValue& element : *entries) {
        const JsonObject* one = element.get<JsonObject>();
        if (one == nullptr) {
            return std::unexpected(missing("pool entry"));
        }
        Parsed<LootEntry> parsed = entry(*one);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }
        built.entries.push_back(std::move(*parsed));
    }
    Parsed<std::vector<LootCondition>> conditions = optionalCondition(object.find("condition"));
    if (!conditions.has_value()) {
        return std::unexpected(conditions.error());
    }
    built.conditions = std::move(*conditions);
    Parsed<std::vector<LootFunction>> functions = modifier(object.find("modifier"));
    if (!functions.has_value()) {
        return std::unexpected(functions.error());
    }
    built.functions = std::move(*functions);
    // parity: `rolls` is required and an int provider, `bonus_rolls` optional and a float one.
    const JsonValue* rolls = object.find("rolls");
    if (rolls == nullptr) {
        return std::unexpected(missing("pool rolls"));
    }
    Parsed<IntProvider> parsedRolls = intProvider(*rolls);
    if (!parsedRolls.has_value()) {
        return std::unexpected(parsedRolls.error());
    }
    built.rolls = std::move(*parsedRolls);
    if (const JsonValue* bonus = object.find("bonus_rolls"); bonus != nullptr) {
        Parsed<FloatProvider> parsed = floatProvider(*bonus);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }
        built.bonusRolls = std::move(*parsed);
    }
    return built;
}

Parsed<LootTable> Reader::table(const JsonObject& object) {
    LootTable built;
    if (const std::vector<JsonValue>* pools = asArray(object.find("pools")); pools != nullptr) {
        for (const JsonValue& element : *pools) {
            const JsonObject* one = element.get<JsonObject>();
            if (one == nullptr) {
                return std::unexpected(missing("loot pool"));
            }
            Parsed<LootPool> parsed = pool(*one);
            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }
            built.pools.push_back(std::move(*parsed));
        }
    }
    // parity: `LootTable.randomSequence`, the name of the stream this table rolls on. Every table
    // the game ships carries one; a datapack's may not, and then it rolls on the level's own random.
    if (const std::optional<std::string> sequence = asString(object.find("random_sequence")); sequence.has_value()) {
        if (const Parsed<Identifier> name = identifierOf(sequence, "random sequence"); name.has_value()) {
            built.randomSequence = *name;
        }
    }
    Parsed<std::vector<LootFunction>> functions = modifier(object.find("modifier"));
    if (!functions.has_value()) {
        return std::unexpected(functions.error());
    }
    built.functions = std::move(*functions);
    return built;
}

} // namespace parts

std::expected<LootTable, LootTableParseError> parse(std::string_view json, LootReferences* references) {
    const std::expected<wire::common::JsonValue, wire::common::JsonError> root = wire::common::parseJson(json);
    if (!root.has_value()) {
        return std::unexpected(LootTableParseError{"Malformed loot table JSON"});
    }
    const wire::common::JsonObject* object = root->get<wire::common::JsonObject>();
    if (object == nullptr) {
        return std::unexpected(parts::missing("loot table object"));
    }
    parts::Reader reader{references};
    return reader.table(*object);
}

std::expected<LootCondition, LootTableParseError> parseCondition(const wire::common::JsonValue& json,
                                                                 LootReferences* references) {
    parts::Reader reader{references};
    return reader.condition(json);
}

std::expected<std::vector<LootFunction>, LootTableParseError> parseModifier(const wire::common::JsonValue& json,
                                                                            LootReferences* references) {
    parts::Reader reader{references};
    return reader.modifier(&json);
}

std::expected<IntProvider, LootTableParseError> parseIntProvider(const wire::common::JsonValue& json,
                                                                 LootReferences* references) {
    parts::Reader reader{references};
    return reader.intProvider(json);
}

std::expected<FloatProvider, LootTableParseError> parseFloatProvider(const wire::common::JsonValue& json,
                                                                     LootReferences* references) {
    parts::Reader reader{references};
    return reader.floatProvider(json);
}

} // namespace lootTableJson

} // namespace basalt::server::world::loot
