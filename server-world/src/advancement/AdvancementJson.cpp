#include "../loot/LootJsonParts.hpp"

#include <basalt/server/world/advancement/AdvancementJson.hpp>
#include <basalt/server/world/loot/LootTableJson.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <utility>

namespace basalt::server::world::advancement::advancementJson {
namespace {

using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;

[[nodiscard]] AdvancementParseError missing(std::string_view what) {
    return AdvancementParseError{std::string{"missing or malformed "} + std::string{what}};
}

[[nodiscard]] const std::string* asString(const JsonValue* value) {
    return value == nullptr ? nullptr : value->get<std::string>();
}

[[nodiscard]] const JsonObject* asObject(const JsonValue* value) {
    return value == nullptr ? nullptr : value->get<JsonObject>();
}

[[nodiscard]] const std::vector<JsonValue>* asArray(const JsonValue* value) {
    return value == nullptr ? nullptr : value->get<std::vector<JsonValue>>();
}

/// Every id in this schema is a resource location, so a malformed one is a malformed file.
[[nodiscard]] std::optional<Identifier> asIdentifier(const JsonValue* value) {
    const std::string* text = asString(value);
    if (text == nullptr) {
        return std::nullopt;
    }
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(*text);
    return parsed.has_value() ? std::optional<Identifier>{*parsed} : std::nullopt;
}

[[nodiscard]] std::vector<Identifier> identifiers(const JsonValue* value) {
    std::vector<Identifier> built;
    const std::vector<JsonValue>* array = asArray(value);
    if (array == nullptr) {
        return built;
    }
    for (const JsonValue& element : *array) {
        if (const std::optional<Identifier> id = asIdentifier(&element); id.has_value()) {
            built.push_back(*id);
        }
    }
    return built;
}

/// `items` is one id, a `#tag`, or a list of ids. parity: `ItemPredicate`'s own `HolderSet` field.
[[nodiscard]] std::optional<ItemMatcher> parseMatcher(const JsonValue& predicate) {
    const JsonObject* object = predicate.get<JsonObject>();
    if (object == nullptr) {
        return std::nullopt;
    }
    const JsonValue* items = object->find("items");
    if (items == nullptr) {
        return std::nullopt;
    }
    if (items->get<std::vector<JsonValue>>() != nullptr) {
        return ItemMatcher{.items = identifiers(items), .tag = std::nullopt};
    }
    const std::string* text = items->get<std::string>();
    if (text == nullptr) {
        return std::nullopt;
    }
    const bool tag = text->starts_with('#');
    const std::expected<Identifier, wire::common::IdentifierError> parsed =
        Identifier::parse(tag ? std::string_view{*text}.substr(1) : std::string_view{*text});
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    if (tag) {
        return ItemMatcher{.items = {}, .tag = *parsed};
    }
    return ItemMatcher{.items = {*parsed}, .tag = std::nullopt};
}

/// The one entry a holder set names: an id, or a list of exactly one. parity: 26.3 turned the
/// recipe, loot table and block of several triggers into holder sets; the model keeps one.
[[nodiscard]] std::optional<Identifier> singleHolder(const JsonValue* value) {
    if (const std::vector<JsonValue>* list = asArray(value); list != nullptr) {
        return list->size() == 1 ? singleHolder(&list->front()) : std::nullopt;
    }
    const std::string* text = asString(value);
    return text == nullptr || text->starts_with('#') ? std::nullopt : asIdentifier(value);
}

namespace parts = loot::lootTableJson::parts;

/// A context-aware predicate: one loot condition as 26.3 writes it, or the older list of them.
[[nodiscard]] std::optional<std::vector<loot::LootCondition>> contextPredicate(const JsonValue& value) {
    std::vector<loot::LootCondition> conditions;
    const auto one = [&conditions](const JsonValue& term) {
        std::expected<loot::LootCondition, loot::LootTableParseError> parsed =
            loot::lootTableJson::parseCondition(term);
        if (!parsed.has_value()) {
            return false;
        }
        conditions.push_back(*std::move(parsed));
        return true;
    };
    if (const std::vector<JsonValue>* list = asArray(&value); list != nullptr) {
        for (const JsonValue& term : *list) {
            if (!one(term)) {
                return std::nullopt;
            }
        }
        return conditions;
    }
    return one(value) ? std::optional{std::move(conditions)} : std::nullopt;
}

/// parity: `DamageSourcePredicate`.
[[nodiscard]] std::optional<DamageSourceCheck> damageSourceOf(const JsonObject& object) {
    DamageSourceCheck check;
    for (std::size_t index = 0; index < object.names().size(); ++index) {
        const std::string& key = object.names()[index];
        const JsonValue& value = object.values()[index];
        if (key == "tags") {
            const std::vector<JsonValue>* tags = asArray(&value);
            if (tags == nullptr) {
                return std::nullopt;
            }
            for (const JsonValue& tag : *tags) {
                const JsonObject* entry = tag.get<JsonObject>();
                const std::string* id = entry == nullptr ? nullptr : asString(entry->find("id"));
                const bool* expected = entry == nullptr || entry->find("expected") == nullptr
                                           ? nullptr
                                           : entry->find("expected")->get<bool>();
                if (id == nullptr || expected == nullptr) {
                    return std::nullopt;
                }
                check.tags.emplace_back(*id, *expected);
            }
        } else if ((key == "direct_entity" || key == "source_entity") && value.get<JsonObject>() != nullptr) {
            auto predicate =
                std::make_shared<const loot::EntityPredicate>(parts::parseEntityPredicate(*value.get<JsonObject>()));
            (key == "direct_entity" ? check.direct : check.source) = std::move(predicate);
        } else if (key == "is_direct" && value.get<bool>() != nullptr) {
            check.isDirect = *value.get<bool>();
        } else {
            return std::nullopt;
        }
    }
    return check;
}

/// parity: `DamagePredicate`.
[[nodiscard]] std::optional<DamageCheck> damageOf(const JsonObject& object) {
    DamageCheck check;
    for (std::size_t index = 0; index < object.names().size(); ++index) {
        const std::string& key = object.names()[index];
        const JsonValue& value = object.values()[index];
        if (key == "dealt" || key == "taken") {
            (key == "dealt" ? check.dealt : check.taken) = parts::doubleBounds(&value);
        } else if (key == "source_entity" && value.get<JsonObject>() != nullptr) {
            check.source =
                std::make_shared<const loot::EntityPredicate>(parts::parseEntityPredicate(*value.get<JsonObject>()));
        } else if (key == "blocked" && value.get<bool>() != nullptr) {
            check.blocked = *value.get<bool>();
        } else if (key == "type" && value.get<JsonObject>() != nullptr) {
            check.type = damageSourceOf(*value.get<JsonObject>());
            if (!check.type.has_value()) {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }
    return check;
}

/// The trigger instances read field by field. parity: each trigger's `TriggerInstance` codec; an
/// unknown field is refused, so a criterion is never granted on less than it asks for.
[[nodiscard]] std::optional<GenericCondition> parseGeneric(std::string_view trigger, const JsonObject* conditions) {
    static constexpr std::array<std::string_view, 13> kEntityFields{
        "player", "entity", "source",   "projectile", "shooter", "bystander", "lightning",
        "cause",  "zombie", "villager", "child",      "parent",  "partner"};
    static constexpr std::array<std::string_view, 3> kItemFields{"item", "fired_from_weapon", "rod"};
    static constexpr std::array<std::string_view, 9> kNumberFields{
        "count",  "unique_entity_types", "signal_strength", "num_bees_inside", "level",
        "levels", "durability",          "delta",           "duration"};
    static constexpr std::array<std::string_view, 4> kIdFields{"recipes", "recipe_id", "potion", "block"};
    GenericCondition built;
    if (conditions == nullptr) {
        return built;
    }
    for (std::size_t index = 0; index < conditions->names().size(); ++index) {
        const std::string& key = conditions->names()[index];
        const JsonValue& value = conditions->values()[index];
        const JsonObject* object = value.get<JsonObject>();
        if (std::ranges::find(kEntityFields, key) != kEntityFields.end()) {
            std::optional<std::vector<loot::LootCondition>> predicate = contextPredicate(value);
            if (!predicate.has_value()) {
                return std::nullopt;
            }
            built.entities.emplace(key, *std::move(predicate));
        } else if (key == "victims") {
            const std::vector<JsonValue>* victims = asArray(&value);
            if (victims == nullptr) {
                return std::nullopt;
            }
            for (const JsonValue& victim : *victims) {
                std::optional<std::vector<loot::LootCondition>> predicate = contextPredicate(victim);
                if (!predicate.has_value()) {
                    return std::nullopt;
                }
                built.victims.push_back(*std::move(predicate));
            }
        } else if (std::ranges::find(kItemFields, key) != kItemFields.end() && object != nullptr) {
            built.items.emplace(key, parts::parseItemPredicate(*object));
        } else if (key == "count" && trigger == "minecraft:spear_mobs") {
            // parity: `SpearMobsTrigger`, whose count is a floor rather than a range.
            built.numbers.emplace(key, loot::DoubleBounds{.min = parts::doubleBounds(&value).min});
        } else if (std::ranges::find(kNumberFields, key) != kNumberFields.end() ||
                   (key == "distance" && trigger == "minecraft:used_ender_eye")) {
            built.numbers.emplace(key, parts::doubleBounds(&value));
        } else if (std::ranges::find(kIdFields, key) != kIdFields.end()) {
            built.ids.emplace(key, parts::idsOrTag(&value));
        } else if (key == "state") {
            built.state = parts::parseStateMatcher(object);
        } else if (key == "distance" && object != nullptr) {
            built.distance = loot::DistancePredicate{.x = parts::doubleBounds(object->find("x")),
                                                     .y = parts::doubleBounds(object->find("y")),
                                                     .z = parts::doubleBounds(object->find("z")),
                                                     .horizontal = parts::doubleBounds(object->find("horizontal")),
                                                     .absolute = parts::doubleBounds(object->find("absolute"))};
        } else if (key == "start_position" && object != nullptr) {
            built.startPosition = std::make_shared<const loot::LocationPredicate>(parts::parseLocation(object));
        } else if (key == "effects" && object != nullptr) {
            for (std::size_t at = 0; at < object->names().size(); ++at) {
                const JsonObject* wanted = object->values()[at].get<JsonObject>();
                loot::EffectRequirement requirement{.effect = object->names()[at]};
                if (wanted != nullptr) {
                    requirement.amplifier = parts::doubleBounds(wanted->find("amplifier"));
                    requirement.duration = parts::doubleBounds(wanted->find("duration"));
                    requirement.ambient = parts::asBool(wanted->find("ambient"));
                    requirement.visible = parts::asBool(wanted->find("visible"));
                }
                built.effects.push_back(std::move(requirement));
            }
        } else if (key == "damage" && object != nullptr) {
            built.damage = damageOf(*object);
            if (!built.damage.has_value()) {
                return std::nullopt;
            }
        } else if (key == "killing_blow" && object != nullptr) {
            built.killingBlow = damageSourceOf(*object);
            if (!built.killingBlow.has_value()) {
                return std::nullopt;
            }
        } else if (key == "ingredients") {
            const std::vector<JsonValue>* ingredients = asArray(&value);
            if (ingredients == nullptr) {
                return std::nullopt;
            }
            for (const JsonValue& ingredient : *ingredients) {
                if (const JsonObject* predicate = ingredient.get<JsonObject>(); predicate != nullptr) {
                    built.ingredients.push_back(parts::parseItemPredicate(*predicate));
                }
            }
        } else if (key == "location") {
            built.location = contextPredicate(value);
            if (!built.location.has_value()) {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }
    return built;
}

/// The triggers kept on their own shape: indexed by item or recipe, or naming only ids.
[[nodiscard]] bool isSpecialized(std::string_view name) {
    return name == "minecraft:inventory_changed" || name == "minecraft:recipe_unlocked" ||
           name == "minecraft:recipe_crafted" || name == "minecraft:player_generates_container_loot" ||
           name == "minecraft:changed_dimension";
}

/// The condition a trigger fires under, for the triggers this server can observe. Anything else is
/// `Unsupported` and never fires, rather than firing on the bare trigger and handing out an
/// advancement nobody earned.
[[nodiscard]] CriterionCondition parseCondition(const Identifier& trigger, const JsonObject* conditions) {
    const std::string name = trigger.toString();
    if (!isSpecialized(name)) {
        // parity: `impossible` never fires, which is what an unsupported criterion is too.
        std::optional<GenericCondition> generic =
            name == "minecraft:impossible" ? std::nullopt : parseGeneric(name, conditions);
        if (!generic.has_value()) {
            return CriterionCondition{};
        }
        return CriterionCondition{.kind = ConditionKind::Generic,
                                  .generic = std::make_shared<const GenericCondition>(*std::move(generic))};
    }
    if (name == "minecraft:inventory_changed") {
        const std::vector<JsonValue>* items = conditions == nullptr ? nullptr : asArray(conditions->find("items"));
        if (items == nullptr) {
            return CriterionCondition{};
        }
        CriterionCondition built{.kind = ConditionKind::InventoryItems, .matchers = {}, .recipe = std::nullopt};
        for (const JsonValue& predicate : *items) {
            if (std::optional<ItemMatcher> matcher = parseMatcher(predicate); matcher.has_value()) {
                built.matchers.push_back(std::move(*matcher));
            }
        }
        return built;
    }
    if (name == "minecraft:recipe_unlocked") {
        const std::optional<Identifier> recipe =
            conditions == nullptr ? std::nullopt : singleHolder(conditions->find("recipes"));
        if (!recipe.has_value()) {
            return CriterionCondition{};
        }
        return CriterionCondition{.kind = ConditionKind::RecipeUnlocked, .matchers = {}, .recipe = recipe};
    }
    if (name == "minecraft:player_generates_container_loot") {
        const std::optional<Identifier> table =
            conditions == nullptr ? std::nullopt : singleHolder(conditions->find("loot_tables"));
        return table.has_value() ? CriterionCondition{.kind = ConditionKind::ContainerLoot, .lootTable = table}
                                 : CriterionCondition{};
    }
    if (name == "minecraft:changed_dimension") {
        CriterionCondition built{.kind = ConditionKind::ChangedDimension};
        if (conditions != nullptr) {
            built.fromDimension = asIdentifier(conditions->find("from"));
            built.toDimension = asIdentifier(conditions->find("to"));
        }
        return built;
    }
    // parity: `RecipeCraftedTrigger`. A criterion that also names the ingredients is left
    // unsupported: what was in the grid is gone by the time the result is taken.
    if (name == "minecraft:recipe_crafted") {
        const std::optional<Identifier> recipe =
            conditions == nullptr ? std::nullopt : singleHolder(conditions->find("recipes"));
        if (!recipe.has_value() || conditions->find("ingredients") != nullptr) {
            return CriterionCondition{};
        }
        return CriterionCondition{.kind = ConditionKind::RecipeCrafted, .recipe = recipe};
    }
    return CriterionCondition{};
}

[[nodiscard]] std::optional<AdvancementDisplay> parseDisplay(const JsonObject& display) {
    const JsonValue* title = display.find("title");
    if (title == nullptr) {
        return std::nullopt;
    }
    const JsonValue* description = display.find("description");
    const std::string* frame = asString(display.find("frame"));
    // parity: the frozen Kotlin reads this off `JsonPrimitive.content`, so the file may spell it as a
    // JSON boolean or as a quoted one, and `String.toBoolean` accepts any casing of `true`.
    bool announceToChat = true;
    if (const JsonValue* announce = display.find("announce_to_chat"); announce != nullptr) {
        if (const bool* flag = announce->get<bool>(); flag != nullptr) {
            announceToChat = *flag;
        } else if (const std::string* text = announce->get<std::string>(); text != nullptr) {
            std::string lowered = *text;
            for (char& character : lowered) {
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }
            announceToChat = lowered == "true";
        } else {
            announceToChat = false;
        }
    }
    return AdvancementDisplay{
        .title = wire::text::componentJson::deserialize(*title),
        .description = description == nullptr ? wire::text::Component::empty()
                                              : wire::text::componentJson::deserialize(*description),
        .type = frame == nullptr ? "task" : *frame,
        .announceToChat = announceToChat,
    };
}

[[nodiscard]] AdvancementRewards parseRewards(const JsonObject& rewards) {
    std::int32_t experience = 0;
    if (const JsonValue* value = rewards.find("experience"); value != nullptr) {
        if (const std::int64_t* whole = value->get<std::int64_t>(); whole != nullptr) {
            experience = static_cast<std::int32_t>(*whole);
        } else if (const std::string* text = value->get<std::string>(); text != nullptr) {
            std::from_chars(text->data(), text->data() + text->size(), experience);
        }
    }
    return AdvancementRewards{.experience = experience,
                              .loot = identifiers(rewards.find("loot")),
                              .recipes = identifiers(rewards.find("recipes"))};
}

} // namespace

std::expected<Advancement, AdvancementParseError> parse(std::string_view json) {
    const std::expected<JsonValue, wire::common::JsonError> parsed = wire::common::parseJson(json);
    if (!parsed.has_value()) {
        return std::unexpected(AdvancementParseError{"Malformed advancement JSON"});
    }
    const JsonObject* root = parsed->get<JsonObject>();
    if (root == nullptr) {
        return std::unexpected(missing("advancement object"));
    }
    const JsonObject* criteria = asObject(root->find("criteria"));
    if (criteria == nullptr) {
        return std::unexpected(missing("criteria"));
    }

    Advancement built;
    for (std::size_t index = 0; index < criteria->names().size(); ++index) {
        const JsonObject* criterion = criteria->values()[index].get<JsonObject>();
        if (criterion == nullptr) {
            return std::unexpected(missing("criterion"));
        }
        const std::optional<Identifier> trigger = asIdentifier(criterion->find("trigger"));
        if (!trigger.has_value()) {
            return std::unexpected(missing("trigger"));
        }
        built.criteria.emplace_back(
            criteria->names()[index],
            AdvancementCriterion{.trigger = *trigger,
                                 .condition = parseCondition(*trigger, asObject(criterion->find("conditions")))});
    }

    if (const std::vector<JsonValue>* groups = asArray(root->find("requirements")); groups != nullptr) {
        for (const JsonValue& group : *groups) {
            std::vector<std::string> names;
            if (const std::vector<JsonValue>* members = group.get<std::vector<JsonValue>>(); members != nullptr) {
                for (const JsonValue& member : *members) {
                    if (const std::string* name = member.get<std::string>(); name != nullptr) {
                        names.push_back(*name);
                    }
                }
            }
            built.requirements.groups.push_back(std::move(names));
        }
    } else {
        for (const std::pair<std::string, AdvancementCriterion>& entry : built.criteria) {
            built.requirements.groups.push_back({entry.first});
        }
    }

    built.parent = asIdentifier(root->find("parent"));
    if (const JsonObject* rewards = asObject(root->find("rewards")); rewards != nullptr) {
        built.rewards = parseRewards(*rewards);
    }
    if (const JsonObject* display = asObject(root->find("display")); display != nullptr) {
        built.display = parseDisplay(*display);
    }
    return built;
}

} // namespace basalt::server::world::advancement::advancementJson
