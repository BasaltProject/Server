#include <basalt/server/world/advancement/AdvancementJson.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

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

/// The entity types one `entity_properties` list names. Only the type is read: a predicate that asks
/// for equipment or components is one this build cannot answer, and a criterion that never fires is
/// better than one granted to the wrong animal. `ok` goes false for anything else.
[[nodiscard]] std::vector<std::string> parseEntityTypes(const std::vector<JsonValue>& list, bool& ok) {
    std::vector<std::string> types;
    for (const JsonValue& element : list) {
        const JsonObject* check = element.get<JsonObject>();
        const JsonObject* predicate = check == nullptr ? nullptr : asObject(check->find("predicate"));
        const std::string* type = predicate == nullptr || predicate->size() != 1
                                      ? nullptr
                                      : asString(predicate->find("minecraft:entity_type"));
        if (type == nullptr) {
            ok = false;
            return {};
        }
        types.push_back(*type);
    }
    return types;
}

/// The condition a trigger fires under, for the triggers this server can observe. Anything else is
/// `Unsupported` and never fires, rather than firing on the bare trigger and handing out an
/// advancement nobody earned.
[[nodiscard]] CriterionCondition parseCondition(const Identifier& trigger, const JsonObject* conditions) {
    const std::string name = trigger.toString();
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
            conditions == nullptr ? std::nullopt : asIdentifier(conditions->find("recipe"));
        if (!recipe.has_value()) {
            return CriterionCondition{};
        }
        return CriterionCondition{.kind = ConditionKind::RecipeUnlocked, .matchers = {}, .recipe = recipe};
    }
    // parity: the entity predicate of `player_killed_entity`, `tame_animal` and `summoned_entity`.
    if (name == "minecraft:player_killed_entity" || name == "minecraft:tame_animal" ||
        name == "minecraft:summoned_entity") {
        const ConditionKind kind = name == "minecraft:player_killed_entity" ? ConditionKind::KilledEntity
                                   : name == "minecraft:tame_animal"        ? ConditionKind::TamedAnimal
                                                                            : ConditionKind::SummonedEntity;
        const std::vector<JsonValue>* entity = conditions == nullptr ? nullptr : asArray(conditions->find("entity"));
        CriterionCondition built{.kind = kind};
        if (entity == nullptr) {
            return conditions == nullptr || conditions->empty() ? built : CriterionCondition{};
        }
        bool ok = true;
        built.entityTypes = parseEntityTypes(*entity, ok);
        return ok ? built : CriterionCondition{};
    }
    if (name == "minecraft:player_generates_container_loot") {
        const std::optional<Identifier> table =
            conditions == nullptr ? std::nullopt : asIdentifier(conditions->find("loot_table"));
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
    if (name == "minecraft:consume_item" || name == "minecraft:filled_bucket") {
        const JsonValue* item = conditions == nullptr ? nullptr : conditions->find("item");
        CriterionCondition built{.kind = name == "minecraft:consume_item" ? ConditionKind::ConsumedItem
                                                                          : ConditionKind::FilledBucket};
        if (item == nullptr) {
            return built;
        }
        std::optional<ItemMatcher> matcher = parseMatcher(*item);
        if (!matcher.has_value()) {
            return CriterionCondition{};
        }
        built.matchers.push_back(std::move(*matcher));
        return built;
    }
    // parity: `RecipeCraftedTrigger`. A criterion that also names the ingredients is left
    // unsupported: what was in the grid is gone by the time the result is taken.
    if (name == "minecraft:recipe_crafted") {
        const std::optional<Identifier> recipe =
            conditions == nullptr ? std::nullopt : asIdentifier(conditions->find("recipe_id"));
        if (!recipe.has_value() || conditions->find("ingredients") != nullptr) {
            return CriterionCondition{};
        }
        return CriterionCondition{.kind = ConditionKind::RecipeCrafted, .recipe = recipe};
    }
    // parity: `PlayerTrigger` as `location`, whose predicate this build reads only as far as one
    // biome or one structure: the two `adventuring_time` and `explore_nether` ask for, plus the four
    // structure ones. Anything else (equipment, what the player is stepping on) is left unsupported.
    if (name == "minecraft:location") {
        const std::vector<JsonValue>* player = conditions == nullptr ? nullptr : asArray(conditions->find("player"));
        if (player == nullptr || player->size() != 1) {
            return CriterionCondition{};
        }
        const JsonObject* term = player->front().get<JsonObject>();
        const JsonObject* predicate = term == nullptr ? nullptr : asObject(term->find("predicate"));
        const JsonObject* where =
            predicate == nullptr || predicate->size() != 1 ? nullptr : asObject(predicate->find("minecraft:location"));
        if (where == nullptr || where->size() != 1) {
            return CriterionCondition{};
        }
        CriterionCondition built{.kind = ConditionKind::AtLocation};
        built.biome = asIdentifier(where->find("biomes"));
        built.structure = asIdentifier(where->find("structures"));
        return built.biome.has_value() || built.structure.has_value() ? built : CriterionCondition{};
    }
    // parity: `EnterBlockTrigger`. A criterion that also names block properties is one this build
    // cannot answer, so it is left unsupported rather than granted on the block alone.
    if (name == "minecraft:enter_block") {
        const std::optional<Identifier> block =
            conditions == nullptr ? std::nullopt : asIdentifier(conditions->find("block"));
        if (!block.has_value() || conditions->find("state") != nullptr) {
            return CriterionCondition{};
        }
        return CriterionCondition{.kind = ConditionKind::EnteredBlock, .block = block};
    }
    // parity: `BredAnimalsTrigger`: the newborn, or the pair that made it.
    if (name == "minecraft:bred_animals") {
        CriterionCondition built{.kind = ConditionKind::BredAnimals};
        if (conditions == nullptr || conditions->empty()) {
            return built;
        }
        bool ok = true;
        const auto read = [&conditions, &ok](std::string_view key) {
            const std::vector<JsonValue>* list = asArray(conditions->find(key));
            if (list == nullptr) {
                ok = ok && conditions->find(key) == nullptr;
                return std::vector<std::string>{};
            }
            return parseEntityTypes(*list, ok);
        };
        built.entityTypes = read("child");
        built.parentTypes = read("parent");
        built.partnerTypes = read("partner");
        return ok ? built : CriterionCondition{};
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
