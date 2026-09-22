#include "LootJsonParts.hpp"

#include <basalt/wire/nbt/JsonToNbt.hpp>

#include <charconv>
#include <cstdint>
#include <utility>

namespace basalt::server::world::loot::lootTableJson::parts {

using wire::common::JsonObject;
using wire::common::JsonValue;

LootTableParseError missing(std::string_view what) {
    return LootTableParseError{std::string{"missing or malformed "} + std::string{what}};
}

const JsonObject* asObject(const JsonValue* value) { return value == nullptr ? nullptr : value->get<JsonObject>(); }

const std::vector<JsonValue>* asArray(const JsonValue* value) {
    return value == nullptr ? nullptr : value->get<std::vector<JsonValue>>();
}

std::optional<std::string> asString(const JsonValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const std::string* text = value->get<std::string>(); text != nullptr) {
        return *text;
    }
    return std::nullopt;
}

/// A JSON number as read by a schema that also accepts it written as a string.
std::optional<double> asNumber(const JsonValue* value) {
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

std::optional<bool> asBool(const JsonValue* value) {
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

std::optional<std::string> typeOf(const JsonObject& object) {
    std::optional<std::string> type = asString(object.find("type"));
    if (type.has_value() && type->find(':') == std::string::npos) {
        type = "minecraft:" + *type;
    }
    return type;
}

StateMatcher parseStateMatcher(const JsonObject* object) {
    StateMatcher matcher;
    if (object == nullptr) {
        return matcher;
    }
    for (std::size_t index = 0; index < object->names().size(); ++index) {
        StateMatcher::Property property{.name = object->names()[index]};
        const JsonValue& value = object->values()[index];
        if (const std::optional<std::string> exact = asString(&value); exact.has_value()) {
            property.exact = exact;
        } else if (const JsonObject* range = value.get<JsonObject>(); range != nullptr) {
            property.min = asString(range->find("min"));
            property.max = asString(range->find("max"));
        }
        matcher.properties.push_back(std::move(property));
    }
    return matcher;
}

/// An id list, a single id, or a `#tag`, as a registry holder set is written.
std::vector<std::string> idsOrTag(const JsonValue* value) {
    std::vector<std::string> ids;
    if (const std::optional<std::string> single = asString(value); single.has_value()) {
        ids.push_back(*single);
    } else if (const std::vector<JsonValue>* list = asArray(value); list != nullptr) {
        for (const JsonValue& each : *list) {
            if (const std::optional<std::string> id = asString(&each); id.has_value()) {
                ids.push_back(*id);
            }
        }
    }
    return ids;
}

/// parity: `ItemPredicate`, reading only `items` and the `minecraft:enchantments` sub-predicate.
ItemPredicate parseItemPredicate(const JsonObject& object) {
    ItemPredicate predicate;
    for (std::size_t index = 0; index < object.names().size(); ++index) {
        const std::string& name = object.names()[index];
        const JsonValue& value = object.values()[index];
        if (name == "items") {
            predicate.items = idsOrTag(&value);
            continue;
        }
        const JsonObject* predicates = name == "predicates" ? value.get<JsonObject>() : nullptr;
        if (predicates == nullptr) {
            predicate.unsupported = true;
            continue;
        }
        for (std::size_t at = 0; at < predicates->names().size(); ++at) {
            const std::vector<JsonValue>* list = predicates->names()[at] == "minecraft:enchantments"
                                                     ? predicates->values()[at].get<std::vector<JsonValue>>()
                                                     : nullptr;
            if (list == nullptr) {
                predicate.unsupported = true;
                continue;
            }
            for (const JsonValue& each : *list) {
                const JsonObject* entry = each.get<JsonObject>();
                if (entry == nullptr) {
                    continue;
                }
                EnchantmentRequirement requirement{.enchantments = idsOrTag(entry->find("enchantments"))};
                if (const JsonObject* levels = asObject(entry->find("levels")); levels != nullptr) {
                    if (const std::optional<double> min = asNumber(levels->find("min")); min.has_value()) {
                        requirement.min = static_cast<std::int32_t>(*min);
                    }
                    if (const std::optional<double> max = asNumber(levels->find("max")); max.has_value()) {
                        requirement.max = static_cast<std::int32_t>(*max);
                    }
                } else if (const std::optional<double> exact = asNumber(entry->find("levels")); exact.has_value()) {
                    requirement.min = static_cast<std::int32_t>(*exact);
                    requirement.max = static_cast<std::int32_t>(*exact);
                }
                predicate.enchantments.push_back(std::move(requirement));
            }
        }
    }
    return predicate;
}

/// parity: `EntityPredicate`, the sub-predicates the loot tables use; anything else never matches.
EntityPredicate parseEntityPredicate(const JsonObject& object) {
    EntityPredicate predicate;
    for (std::size_t index = 0; index < object.names().size(); ++index) {
        const std::string& name = object.names()[index];
        const JsonValue& value = object.values()[index];
        const JsonObject* inner = value.get<JsonObject>();
        if (name == "minecraft:entity_type") {
            predicate.types = idsOrTag(&value);
        } else if (name == "minecraft:flags" && inner != nullptr) {
            for (std::size_t at = 0; at < inner->names().size(); ++at) {
                const std::optional<bool> flag = asBool(&inner->values()[at]);
                if (inner->names()[at] == "is_on_fire") {
                    predicate.onFire = flag;
                } else if (inner->names()[at] == "is_baby") {
                    predicate.baby = flag;
                } else {
                    predicate.unsupported = true;
                }
            }
        } else if (name == "minecraft:components" && inner != nullptr) {
            for (std::size_t at = 0; at < inner->names().size(); ++at) {
                if (const std::optional<std::string> text = asString(&inner->values()[at]); text.has_value()) {
                    predicate.components.emplace(inner->names()[at], *text);
                } else {
                    predicate.unsupported = true;
                }
            }
        } else if (name == "minecraft:type_specific/sheep" && inner != nullptr) {
            predicate.sheared = asBool(inner->find("sheared"));
        } else if (name == "minecraft:type_specific/cube_mob" && inner != nullptr) {
            if (const std::optional<double> size = asNumber(inner->find("size")); size.has_value()) {
                predicate.cubeSize = static_cast<std::int32_t>(*size);
            }
        } else if (name == "minecraft:type_specific/raider" && inner != nullptr) {
            predicate.captain = asBool(inner->find("is_captain"));
        } else if (name == "minecraft:vehicle" && inner != nullptr) {
            predicate.vehicle = std::make_shared<EntityPredicate>(parseEntityPredicate(*inner));
        } else if (name == "minecraft:equipment" && inner != nullptr) {
            const JsonObject* mainhand = asObject(inner->find("mainhand"));
            if (mainhand != nullptr && inner->size() == 1) {
                predicate.mainhand = parseItemPredicate(*mainhand);
            } else {
                predicate.unsupported = true;
            }
        } else {
            predicate.unsupported = true;
        }
    }
    return predicate;
}

/// parity: `LevelBasedValue`: a plain number, or `linear`.
LevelBasedValue parseLevelBased(const JsonValue* value) {
    if (const std::optional<double> constant = asNumber(value); constant.has_value()) {
        return LevelBasedValue{.base = static_cast<float>(*constant)};
    }
    const JsonObject* object = asObject(value);
    if (object == nullptr) {
        return {};
    }
    if (asString(object->find("type")) == std::optional<std::string>{"minecraft:linear"}) {
        return LevelBasedValue{.base = static_cast<float>(asNumber(object->find("base")).value_or(0.0)),
                               .perLevelAboveFirst =
                                   static_cast<float>(asNumber(object->find("per_level_above_first")).value_or(0.0))};
    }
    return LevelBasedValue{.base = static_cast<float>(asNumber(object->find("value")).value_or(0.0))};
}

/// The functions that only write components: `set_components` as given, `set_name` into the name
/// it targets, and `set_potion` into its own.
Parsed<LootFunction> parseComponentSetter(std::string_view type, const JsonObject& function) {
    wire::nbt::NbtCompound components;
    std::vector<std::string> removed;
    const auto put = [&components](std::string_view key, const JsonValue* value) {
        if (value == nullptr) {
            return true;
        }
        std::expected<wire::nbt::NbtTag, wire::nbt::JsonToNbtError> tag = wire::nbt::jsonToNbt(*value);
        if (!tag.has_value()) {
            return false;
        }
        components.set(key, std::move(*tag));
        return true;
    };
    bool ok = true;
    if (type == "minecraft:set_name") {
        const std::optional<std::string> target = asString(function.find("target"));
        ok = put(target == std::optional<std::string>{"item_name"} ? "minecraft:item_name" : "minecraft:custom_name",
                 function.find("name"));
    } else if (type == "minecraft:set_potion") {
        wire::nbt::NbtCompound contents;
        if (const std::optional<std::string> potion = asString(function.find("id")); potion.has_value()) {
            contents.putString("potion", *potion);
        }
        components.set("minecraft:potion_contents", wire::nbt::NbtTag::ofCompound(std::move(contents)));
    } else if (const JsonObject* given = asObject(function.find("components")); given != nullptr) {
        for (std::size_t index = 0; index < given->names().size() && ok; ++index) {
            const std::string& name = given->names()[index];
            if (name.starts_with('!')) {
                removed.push_back(name.substr(1));
            } else {
                ok = put(name, &given->values()[index]);
            }
        }
    }
    if (!ok) {
        return std::unexpected(LootTableParseError{"Component value NBT cannot hold"});
    }
    return LootFunction::setComponents(std::move(components), std::move(removed));
}

} // namespace basalt::server::world::loot::lootTableJson::parts
