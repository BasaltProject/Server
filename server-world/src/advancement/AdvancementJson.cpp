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

/// The `type` of a condition, with the default namespace filled in.
[[nodiscard]] std::string conditionType(const JsonObject& condition) {
    const std::string* type = asString(condition.find("type"));
    if (type == nullptr) {
        return {};
    }
    return type->find(':') == std::string::npos ? "minecraft:" + *type : *type;
}

/// The terms of a criterion's condition. parity: 26.3 made it one predicate holder where 26.2 had a
/// list, and a list of several is written as `all_of`, so this unwraps that one level. `ok` goes
/// false for a term that is a named predicate or a tag, which this build does not read here.
[[nodiscard]] std::vector<const JsonObject*> conditionTerms(const JsonValue* value, bool& ok) {
    std::vector<const JsonObject*> terms;
    const JsonObject* condition = asObject(value);
    if (condition == nullptr) {
        ok = false;
        return terms;
    }
    if (conditionType(*condition) != "minecraft:all_of") {
        terms.push_back(condition);
        return terms;
    }
    const std::vector<JsonValue>* list = asArray(condition->find("terms"));
    if (list == nullptr) {
        ok = false;
        return terms;
    }
    for (const JsonValue& element : *list) {
        const JsonObject* term = element.get<JsonObject>();
        if (term == nullptr) {
            ok = false;
            return {};
        }
        terms.push_back(term);
    }
    return terms;
}

/// The entity type one `entity_properties` condition names. Only the type is read: a predicate that
/// asks for equipment or components is one this build cannot answer, and a criterion that never
/// fires is better than one granted to the wrong animal. `ok` goes false for anything else.
[[nodiscard]] std::vector<std::string> parseEntityTypes(const JsonValue* condition, bool& ok) {
    std::vector<std::string> types;
    for (const JsonObject* check : conditionTerms(condition, ok)) {
        const JsonObject* predicate =
            conditionType(*check) == "minecraft:entity_properties" ? asObject(check->find("predicate")) : nullptr;
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

/// The one entry a holder set names: an id, or a list of exactly one. parity: 26.3 turned the
/// recipe, loot table and block of several triggers into holder sets; the model keeps one.
[[nodiscard]] std::optional<Identifier> singleHolder(const JsonValue* value) {
    if (const std::vector<JsonValue>* list = asArray(value); list != nullptr) {
        return list->size() == 1 ? singleHolder(&list->front()) : std::nullopt;
    }
    const std::string* text = asString(value);
    return text == nullptr || text->starts_with('#') ? std::nullopt : asIdentifier(value);
}

/// The blocks a `blocks` holder set names: one id or a list of them. A `#tag` is left unsupported,
/// since a matcher with no blocks would match every block.
[[nodiscard]] std::optional<BlockMatcher> blocksOf(const JsonValue* blocks) {
    if (asArray(blocks) != nullptr) {
        return BlockMatcher{.blocks = identifiers(blocks)};
    }
    const std::string* text = asString(blocks);
    const std::optional<Identifier> one =
        text == nullptr || text->starts_with('#') ? std::nullopt : asIdentifier(blocks);
    return one.has_value() ? std::optional{BlockMatcher{.blocks = {*one}}} : std::nullopt;
}

/// A JSON number as a whole one. Integers arrive as `std::int64_t` and only the ones written with a
/// decimal point as `double`, so a reader that asks for one shape misses half the files.
[[nodiscard]] std::optional<std::int32_t> asWholeNumber(const JsonValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const std::int64_t* whole = value->get<std::int64_t>(); whole != nullptr) {
        return static_cast<std::int32_t>(*whole);
    }
    const double* real = value->get<double>();
    return real == nullptr ? std::nullopt : std::optional{static_cast<std::int32_t>(*real)};
}

/// The state values a criterion asks of a block, as `match_block` and `location_check` both spell
/// them: a flat object of strings. `ok` goes false for a value that is not one.
[[nodiscard]] std::vector<std::pair<std::string, std::string>> parseStateValues(const JsonObject* state, bool& ok) {
    std::vector<std::pair<std::string, std::string>> values;
    if (state == nullptr) {
        return values;
    }
    for (std::size_t index = 0; index < state->names().size(); ++index) {
        const std::string* text = state->values()[index].get<std::string>();
        if (text == nullptr) {
            ok = false;
            return {};
        }
        values.emplace_back(state->names()[index], *text);
    }
    return values;
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
            conditions == nullptr ? std::nullopt : singleHolder(conditions->find("recipes"));
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
        const JsonValue* entity = conditions == nullptr ? nullptr : conditions->find("entity");
        CriterionCondition built{.kind = kind};
        if (entity == nullptr) {
            return conditions == nullptr || conditions->empty() ? built : CriterionCondition{};
        }
        bool ok = true;
        built.entityTypes = parseEntityTypes(entity, ok);
        return ok ? built : CriterionCondition{};
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
    // parity: `FishingRodHookedTrigger` also knows the rod and what was hooked; every criterion in
    // the jar names only the item, so anything else is left unsupported.
    if (name == "minecraft:consume_item" || name == "minecraft:filled_bucket" ||
        name == "minecraft:fishing_rod_hooked") {
        const JsonValue* item = conditions == nullptr ? nullptr : conditions->find("item");
        if (name == "minecraft:fishing_rod_hooked" && conditions != nullptr &&
            (conditions->find("rod") != nullptr || conditions->find("entity") != nullptr)) {
            return CriterionCondition{};
        }
        CriterionCondition built{.kind = name == "minecraft:consume_item"    ? ConditionKind::ConsumedItem
                                         : name == "minecraft:filled_bucket" ? ConditionKind::FilledBucket
                                                                             : ConditionKind::FishingHooked};
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
    // parity: the three triggers whose criteria in the jar ask for nothing at all: doing the thing is
    // the whole condition. A file that does add one is left unsupported rather than granted blind.
    if (name == "minecraft:slept_in_bed" || name == "minecraft:villager_trade" || name == "minecraft:enchanted_item") {
        const ConditionKind kind = name == "minecraft:slept_in_bed"     ? ConditionKind::SleptInBed
                                   : name == "minecraft:villager_trade" ? ConditionKind::VillagerTrade
                                                                        : ConditionKind::EnchantedItem;
        return conditions == nullptr || conditions->empty() ? CriterionCondition{.kind = kind} : CriterionCondition{};
    }
    // parity: `ConstructBeaconTrigger`, whose `level` is either a number or a range.
    if (name == "minecraft:construct_beacon") {
        const JsonValue* level = conditions == nullptr ? nullptr : conditions->find("level");
        CriterionCondition built{.kind = ConditionKind::ConstructedBeacon};
        if (level == nullptr) {
            return conditions == nullptr || conditions->empty() ? built : CriterionCondition{};
        }
        if (const std::optional<std::int32_t> exact = asWholeNumber(level);
            exact.has_value() && level->get<JsonObject>() == nullptr) {
            built.minLevel = *exact;
            built.maxLevel = *exact;
            return built;
        }
        const JsonObject* range = level->get<JsonObject>();
        if (range == nullptr) {
            return CriterionCondition{};
        }
        const auto bound = [range](std::string_view key) { return asWholeNumber(range->find(key)); };
        built.minLevel = bound("min");
        built.maxLevel = bound("max");
        return built.minLevel.has_value() || built.maxLevel.has_value() ? built : CriterionCondition{};
    }
    // parity: `ItemUsedOnLocationTrigger` as `placed_block`: one `match_block` term, which names the
    // block and may name state values. The criteria that also ask what is in the squares around (the
    // chiseled bookshelf and its comparator, the creaking heart) are left unsupported.
    if (name == "minecraft:placed_block") {
        bool ok = true;
        const std::vector<const JsonObject*> terms =
            conditionTerms(conditions == nullptr ? nullptr : conditions->find("location"), ok);
        if (!ok || terms.size() != 1 || conditionType(*terms.front()) != "minecraft:match_block") {
            return CriterionCondition{};
        }
        const JsonObject& term = *terms.front();
        std::optional<BlockMatcher> matcher = blocksOf(term.find("blocks"));
        for (const std::string& key : term.names()) {
            ok = ok && (key == "type" || key == "blocks" || key == "state");
        }
        if (!matcher.has_value() || !ok) {
            return CriterionCondition{};
        }
        matcher->properties = parseStateValues(asObject(term.find("state")), ok);
        return ok ? CriterionCondition{.kind = ConditionKind::PlacedBlock, .blockMatcher = std::move(*matcher)}
                  : CriterionCondition{};
    }
    // parity: `ItemUsedOnLocationTrigger` as `item_used_on_block`: an `all_of` of a `location_check`
    // naming the block and a `match_tool` naming the item. A check with an offset asks about another
    // square, which this build does not answer, so those are left unsupported.
    if (name == "minecraft:item_used_on_block") {
        bool ok = true;
        const std::vector<const JsonObject*> terms =
            conditionTerms(conditions == nullptr ? nullptr : conditions->find("location"), ok);
        if (!ok || terms.size() != 2) {
            return CriterionCondition{};
        }
        CriterionCondition built{.kind = ConditionKind::ItemUsedOnBlock};
        for (const JsonObject* term : terms) {
            const std::string condition = conditionType(*term);
            const JsonObject* predicate = asObject(term->find("predicate"));
            if (predicate == nullptr || term->size() != 2) {
                return CriterionCondition{};
            }
            if (condition == "minecraft:location_check") {
                const JsonObject* block = asObject(predicate->find("block"));
                std::optional<BlockMatcher> matcher = block == nullptr ? std::nullopt : blocksOf(block->find("blocks"));
                if (predicate->size() != 1 || !matcher.has_value()) {
                    return CriterionCondition{};
                }
                matcher->properties = parseStateValues(asObject(block->find("state")), ok);
                built.blockMatcher = std::move(matcher);
                continue;
            }
            if (condition != "minecraft:match_tool") {
                return CriterionCondition{};
            }
            std::optional<ItemMatcher> matcher = parseMatcher(JsonValue{*predicate});
            if (!matcher.has_value()) {
                return CriterionCondition{};
            }
            built.matchers.push_back(std::move(*matcher));
        }
        return ok && built.blockMatcher.has_value() && !built.matchers.empty() ? built : CriterionCondition{};
    }
    // parity: `PlayerInteractTrigger`, which names one entity and the item held. Beyond the type and
    // whether it is a baby, an entity predicate here asks about components or equipment, which this
    // build cannot answer, so those criteria stay unsupported.
    if (name == "minecraft:player_interacted_with_entity") {
        bool ok = true;
        const std::vector<const JsonObject*> entity =
            conditionTerms(conditions == nullptr ? nullptr : conditions->find("entity"), ok);
        const JsonValue* item = conditions == nullptr ? nullptr : conditions->find("item");
        if (!ok || entity.size() != 1 || item == nullptr ||
            conditionType(*entity.front()) != "minecraft:entity_properties") {
            return CriterionCondition{};
        }
        const JsonObject* predicate = asObject(entity.front()->find("predicate"));
        const std::string* type = predicate == nullptr ? nullptr : asString(predicate->find("minecraft:entity_type"));
        if (type == nullptr || predicate->size() > 2) {
            return CriterionCondition{};
        }
        CriterionCondition built{.kind = ConditionKind::InteractedWithEntity, .entityTypes = {*type}};
        if (predicate->size() == 2) {
            const JsonObject* flags = asObject(predicate->find("minecraft:flags"));
            const JsonValue* isBaby = flags == nullptr || flags->size() != 1 ? nullptr : flags->find("is_baby");
            const bool* wanted = isBaby == nullptr ? nullptr : isBaby->get<bool>();
            if (wanted == nullptr) {
                return CriterionCondition{};
            }
            built.baby = *wanted;
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
            conditions == nullptr ? std::nullopt : singleHolder(conditions->find("recipes"));
        if (!recipe.has_value() || conditions->find("ingredients") != nullptr) {
            return CriterionCondition{};
        }
        return CriterionCondition{.kind = ConditionKind::RecipeCrafted, .recipe = recipe};
    }
    // parity: `PlayerTrigger` as `location`, whose predicate this build reads only as far as one
    // biome or one structure: the two `adventuring_time` and `explore_nether` ask for, plus the four
    // structure ones. Anything else (equipment, what the player is stepping on) is left unsupported.
    if (name == "minecraft:location") {
        bool ok = true;
        const std::vector<const JsonObject*> player =
            conditionTerms(conditions == nullptr ? nullptr : conditions->find("player"), ok);
        if (!ok || player.size() != 1 || conditionType(*player.front()) != "minecraft:entity_properties") {
            return CriterionCondition{};
        }
        const JsonObject* predicate = asObject(player.front()->find("predicate"));
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
            conditions == nullptr ? std::nullopt : singleHolder(conditions->find("blocks"));
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
            const JsonValue* condition = conditions->find(key);
            return condition == nullptr ? std::vector<std::string>{} : parseEntityTypes(condition, ok);
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
