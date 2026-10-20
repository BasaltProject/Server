#include "LootJsonParts.hpp"

#include <basalt/wire/nbt/JsonToNbt.hpp>

#include <algorithm>
#include <array>
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

namespace {

/// The component predicate types 26.3 registers, which an empty object does not turn into `AnyValue`.
[[nodiscard]] bool isPredicateType(std::string_view key) {
    static constexpr std::array<std::string_view, 15> kTypes{"minecraft:damage",
                                                             "minecraft:enchantments",
                                                             "minecraft:stored_enchantments",
                                                             "minecraft:potion_contents",
                                                             "minecraft:custom_data",
                                                             "minecraft:container",
                                                             "minecraft:bundle_contents",
                                                             "minecraft:firework_explosion",
                                                             "minecraft:fireworks",
                                                             "minecraft:writable_book_content",
                                                             "minecraft:written_book_content",
                                                             "minecraft:attribute_modifiers",
                                                             "minecraft:trim",
                                                             "minecraft:jukebox_playable",
                                                             "minecraft:villager/variant"};
    return std::ranges::find(kTypes, key) != kTypes.end();
}

} // namespace

/// parity: `MinMaxBounds.Doubles`, a number or a `{min, max}`.
DoubleBounds doubleBounds(const JsonValue* value) {
    if (const std::optional<double> exact = asNumber(value); exact.has_value()) {
        return DoubleBounds{.min = exact, .max = exact};
    }
    const JsonObject* object = asObject(value);
    return object == nullptr ? DoubleBounds{}
                             : DoubleBounds{.min = asNumber(object->find("min")), .max = asNumber(object->find("max"))};
}

/// parity: `LocationPredicate`.
LocationPredicate parseLocation(const JsonObject* predicate) {
    LocationPredicate location;
    if (predicate == nullptr) {
        return location;
    }
    for (std::size_t index = 0; index < predicate->names().size(); ++index) {
        const std::string& name = predicate->names()[index];
        const JsonValue& value = predicate->values()[index];
        const JsonObject* object = value.get<JsonObject>();
        if (name == "biomes") {
            location.biomes = idsOrTag(&value);
        } else if (name == "structures") {
            location.structures = idsOrTag(&value);
        } else if (name == "dimension") {
            const auto parsed = wire::common::Identifier::parse(asString(&value).value_or(""));
            location.unsupported = location.unsupported || !parsed.has_value();
            if (parsed.has_value()) {
                location.dimension = *parsed;
            }
        } else if (name == "position" && object != nullptr) {
            location.position = std::array<DoubleBounds, 3>{
                doubleBounds(object->find("x")), doubleBounds(object->find("y")), doubleBounds(object->find("z"))};
        } else if (name == "smokey") {
            location.smokey = asBool(&value);
        } else if (name == "can_see_sky") {
            location.canSeeSky = asBool(&value);
        } else if (name == "light" && object != nullptr) {
            location.hasLight = true;
            const DoubleBounds bounds = doubleBounds(object->find("light"));
            if (bounds.min.has_value()) {
                location.lightMin = static_cast<std::int32_t>(*bounds.min);
            }
            if (bounds.max.has_value()) {
                location.lightMax = static_cast<std::int32_t>(*bounds.max);
            }
        } else if (name == "fluid" && object != nullptr) {
            location.hasFluid = true;
            location.fluids = idsOrTag(object->find("fluids"));
            location.fluidState = parseStateMatcher(asObject(object->find("state")));
        } else if (name == "block" && object != nullptr) {
            location.hasBlock = true;
            location.blocks = idsOrTag(object->find("blocks"));
            location.state = parseStateMatcher(asObject(object->find("state")));
            for (const std::string& key : object->names()) {
                location.unsupported = location.unsupported || (key != "blocks" && key != "state");
            }
        } else {
            location.unsupported = true;
        }
    }
    return location;
}

/// parity: `ItemPredicate`.
ItemPredicate parseItemPredicate(const JsonObject& object) {
    ItemPredicate predicate;
    for (std::size_t index = 0; index < object.names().size(); ++index) {
        const std::string& name = object.names()[index];
        const JsonValue& value = object.values()[index];
        if (name == "items") {
            predicate.items = idsOrTag(&value);
            continue;
        }
        if (name == "count") {
            predicate.count = doubleBounds(&value);
            continue;
        }
        // parity: `DataComponentExactPredicate`, each component held with exactly that value.
        if (const JsonObject* exact = name == "components" ? value.get<JsonObject>() : nullptr; exact != nullptr) {
            for (std::size_t at = 0; at < exact->names().size(); ++at) {
                std::expected<wire::nbt::NbtTag, wire::nbt::JsonToNbtError> tag =
                    wire::nbt::jsonToNbt(exact->values()[at]);
                if (!tag.has_value()) {
                    predicate.unsupported = true;
                    continue;
                }
                predicate.exact.emplace_back(exact->names()[at], *std::move(tag));
            }
            continue;
        }
        const JsonObject* predicates = name == "predicates" ? value.get<JsonObject>() : nullptr;
        if (predicates == nullptr) {
            predicate.unsupported = true;
            continue;
        }
        for (std::size_t at = 0; at < predicates->names().size(); ++at) {
            const std::string& key = predicates->names()[at];
            const bool stored = key == "minecraft:stored_enchantments";
            const std::vector<JsonValue>* list = key == "minecraft:enchantments" || stored
                                                     ? predicates->values()[at].get<std::vector<JsonValue>>()
                                                     : nullptr;
            // parity: `DataComponentPredicate`'s `AnyValue`.
            if (const JsonObject* empty = predicates->values()[at].get<JsonObject>();
                list == nullptr && empty != nullptr && empty->size() == 0 && !isPredicateType(key)) {
                predicate.present.push_back(key);
                continue;
            }
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
                (stored ? predicate.storedEnchantments : predicate.enchantments).push_back(std::move(requirement));
            }
        }
    }
    return predicate;
}

/// parity: `EntityPredicate`.
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
                const std::string& key = inner->names()[at];
                if (key == "is_on_fire") {
                    predicate.onFire = flag;
                } else if (key == "is_baby") {
                    predicate.baby = flag;
                } else if (flag.has_value()) {
                    predicate.flags.emplace(key, *flag);
                } else {
                    predicate.unsupported = true;
                }
            }
        } else if (name == "minecraft:location" && inner != nullptr) {
            predicate.location = std::make_shared<const LocationPredicate>(parseLocation(inner));
        } else if (name == "minecraft:stepping_on" && inner != nullptr) {
            predicate.steppingOn = std::make_shared<const LocationPredicate>(parseLocation(inner));
        } else if (name == "minecraft:distance" && inner != nullptr) {
            predicate.distance = DistancePredicate{.x = doubleBounds(inner->find("x")),
                                                   .y = doubleBounds(inner->find("y")),
                                                   .z = doubleBounds(inner->find("z")),
                                                   .horizontal = doubleBounds(inner->find("horizontal")),
                                                   .absolute = doubleBounds(inner->find("absolute"))};
        } else if (name == "minecraft:passenger" && inner != nullptr) {
            predicate.passenger = std::make_shared<EntityPredicate>(parseEntityPredicate(*inner));
        } else if (name == "minecraft:type_specific/lightning" && inner != nullptr) {
            predicate.blocksSetOnFire = doubleBounds(inner->find("blocks_set_on_fire"));
            predicate.unsupported = predicate.unsupported || inner->find("entity_struck") != nullptr;
        } else if (name == "minecraft:type_specific/player" && inner != nullptr) {
            // parity: `PlayerPredicate`, of which only `looking_at` is read; the rest never match.
            for (std::size_t at = 0; at < inner->names().size(); ++at) {
                const JsonObject* looked = inner->values()[at].get<JsonObject>();
                if (inner->names()[at] == "looking_at" && looked != nullptr) {
                    predicate.lookingAt = std::make_shared<EntityPredicate>(parseEntityPredicate(*looked));
                } else {
                    predicate.unsupported = true;
                }
            }
        } else if (name == "minecraft:effects" && inner != nullptr) {
            for (std::size_t at = 0; at < inner->names().size(); ++at) {
                const JsonObject* wanted = inner->values()[at].get<JsonObject>();
                EffectRequirement requirement{.effect = inner->names()[at]};
                if (wanted != nullptr) {
                    requirement.amplifier = doubleBounds(wanted->find("amplifier"));
                    requirement.duration = doubleBounds(wanted->find("duration"));
                    requirement.ambient = asBool(wanted->find("ambient"));
                    requirement.visible = asBool(wanted->find("visible"));
                }
                predicate.effects.push_back(std::move(requirement));
            }
        } else if (name == "minecraft:components" && inner != nullptr) {
            for (std::size_t at = 0; at < inner->names().size(); ++at) {
                if (const std::optional<std::string> text = asString(&inner->values()[at]); text.has_value()) {
                    predicate.components.emplace(inner->names()[at], *text);
                } else {
                    predicate.unsupported = true;
                }
            }
        } else if (name == "minecraft:predicates" && inner != nullptr) {
            // parity: `VillagerTypePredicate`, the one component predicate an entity is asked about.
            for (std::size_t at = 0; at < inner->names().size(); ++at) {
                if (inner->names()[at] == "minecraft:villager/variant") {
                    predicate.componentsIn.emplace(inner->names()[at], idsOrTag(&inner->values()[at]));
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
            for (std::size_t at = 0; at < inner->names().size(); ++at) {
                const JsonObject* slot = inner->values()[at].get<JsonObject>();
                if (slot == nullptr) {
                    predicate.unsupported = true;
                } else if (inner->names()[at] == "mainhand") {
                    predicate.mainhand = parseItemPredicate(*slot);
                } else {
                    predicate.equipment.emplace(inner->names()[at], parseItemPredicate(*slot));
                }
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
    std::optional<EntityTarget> resolveFor;
    std::vector<std::string> resolveKeys;
    if (type == "minecraft:set_name") {
        resolveFor = entityTargetOf(function.find("entity"));
        const std::optional<std::string> target = asString(function.find("target"));
        resolveKeys.push_back(target == std::optional<std::string>{"item_name"} ? "minecraft:item_name"
                                                                                : "minecraft:custom_name");
    }
    return LootFunction::setComponents(std::move(components), std::move(removed), resolveFor, std::move(resolveKeys));
}

} // namespace basalt::server::world::loot::lootTableJson::parts
