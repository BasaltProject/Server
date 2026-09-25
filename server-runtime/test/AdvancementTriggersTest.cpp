#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/world/advancement/AdvancementJson.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

using basalt::server::runtime::world::AdvancementTriggers;
using basalt::server::runtime::world::PlayerProgress;
using basalt::server::runtime::world::RealAdvancements;
using basalt::server::runtime::world::TriggerDamage;
using basalt::server::runtime::world::TriggerEvent;
using basalt::server::runtime::world::TriggerItem;
using basalt::server::storage::StoredItemStack;
using basalt::server::world::advancement::Advancement;
using basalt::server::world::advancement::AdvancementCriterion;
using basalt::server::world::advancement::AdvancementDisplay;
using basalt::server::world::advancement::ConditionKind;
using basalt::server::world::advancement::CriterionCondition;
using basalt::server::world::advancement::ItemMatcher;
using basalt::server::world::loot::EntityFacts;
using basalt::wire::common::Identifier;
using basalt::wire::text::Component;

namespace {

/// Announced, like almost every advancement the jar ships: one with no display is never announced.
[[nodiscard]] Advancement wantingItems(const std::vector<ItemMatcher>& matchers) {
    Advancement advancement;
    advancement.display = AdvancementDisplay{.title = Component::text("Stone Age"),
                                             .description = Component::text("Mine stone"),
                                             .type = "task",
                                             .announceToChat = true};
    advancement.criteria.emplace_back(
        "has_item", AdvancementCriterion{
                        .trigger = Identifier::vanilla("inventory_changed"),
                        .condition = CriterionCondition{.kind = ConditionKind::InventoryItems, .matchers = matchers}});
    advancement.requirements.groups = {{"has_item"}};
    return advancement;
}

[[nodiscard]] Advancement wantingRecipe(const Identifier& recipe) {
    Advancement advancement;
    advancement.display = AdvancementDisplay{.title = Component::text("Recipe"),
                                             .description = Component::text("Learn it"),
                                             .type = "task",
                                             .announceToChat = true};
    advancement.criteria.emplace_back(
        "learnt",
        AdvancementCriterion{.trigger = Identifier::vanilla("recipe_unlocked"),
                             .condition = CriterionCondition{.kind = ConditionKind::RecipeUnlocked, .recipe = recipe}});
    advancement.requirements.groups = {{"learnt"}};
    return advancement;
}

[[nodiscard]] std::vector<std::optional<StoredItemStack>> carrying(const std::vector<std::string>& items) {
    std::vector<std::optional<StoredItemStack>> inventory;
    inventory.emplace_back(std::nullopt);
    for (const std::string& item : items) {
        inventory.emplace_back(StoredItemStack{.id = item, .count = 1, .components = {}});
    }
    return inventory;
}

/// What the triggers told the world about, for a test to look at.
struct Told {
    std::vector<std::string> announced;
    std::vector<std::int32_t> experience;
    std::vector<Identifier> recipes;

    [[nodiscard]] AdvancementTriggers::Hooks hooks() {
        return AdvancementTriggers::Hooks{
            .itemTagMembers =
                [](const Identifier& tag) {
                    std::set<Identifier> members;
                    if (tag.path() == "planks") {
                        members.insert(Identifier::vanilla("oak_planks"));
                        members.insert(Identifier::vanilla("birch_planks"));
                    }
                    return members;
                },
            .announce = [this](std::string_view player, const Component&) { announced.emplace_back(player); },
            .awardExperience = [this](std::string_view, std::int32_t amount) { experience.push_back(amount); },
            .awardRecipe = [this](std::string_view, const Identifier& recipe) { recipes.push_back(recipe); }};
    }
};

} // namespace

TEST_CASE("picking up the item an advancement wants completes it") {
    std::map<Identifier, Advancement> definitions;
    definitions.emplace(Identifier::vanilla("story/mine_stone"),
                        wantingItems({ItemMatcher{.items = {Identifier::vanilla("stone")}}}));
    const RealAdvancements advancements{std::move(definitions)};
    PlayerProgress progress;
    Told told;
    AdvancementTriggers triggers{advancements, progress, told.hooks()};

    triggers.onInventoryChanged("Steve", "uuid-1", carrying({"minecraft:stone"}));

    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("story/mine_stone")).size() == 1);
    CHECK(told.announced.size() == 1);
    CHECK(told.announced[0] == "Steve");
}

TEST_CASE("an advancement is announced once and no more, however often the inventory moves") {
    std::map<Identifier, Advancement> definitions;
    definitions.emplace(Identifier::vanilla("story/mine_stone"),
                        wantingItems({ItemMatcher{.items = {Identifier::vanilla("stone")}}}));
    const RealAdvancements advancements{std::move(definitions)};
    PlayerProgress progress;
    Told told;
    AdvancementTriggers triggers{advancements, progress, told.hooks()};

    triggers.onInventoryChanged("Steve", "uuid-1", carrying({"minecraft:stone"}));
    triggers.onInventoryChanged("Steve", "uuid-1", carrying({"minecraft:stone", "minecraft:dirt"}));

    CHECK(told.announced.size() == 1);
}

TEST_CASE("every matcher has to be satisfied before the criterion is") {
    std::map<Identifier, Advancement> definitions;
    definitions.emplace(Identifier::vanilla("story/both"),
                        wantingItems({ItemMatcher{.items = {Identifier::vanilla("stone")}},
                                      ItemMatcher{.items = {Identifier::vanilla("stick")}}}));
    const RealAdvancements advancements{std::move(definitions)};
    PlayerProgress progress;
    Told told;
    AdvancementTriggers triggers{advancements, progress, told.hooks()};

    triggers.onInventoryChanged("Steve", "uuid-1", carrying({"minecraft:stone"}));
    CHECK(told.announced.empty());

    triggers.onInventoryChanged("Steve", "uuid-1", carrying({"minecraft:stone", "minecraft:stick"}));
    CHECK(told.announced.size() == 1);
}

TEST_CASE("a matcher written as a tag is satisfied by anything in it") {
    std::map<Identifier, Advancement> definitions;
    definitions.emplace(Identifier::vanilla("story/planks"),
                        wantingItems({ItemMatcher{.tag = Identifier::vanilla("planks")}}));
    const RealAdvancements advancements{std::move(definitions)};
    PlayerProgress progress;
    Told told;
    AdvancementTriggers triggers{advancements, progress, told.hooks()};

    triggers.onInventoryChanged("Steve", "uuid-1", carrying({"minecraft:birch_planks"}));

    CHECK(told.announced.size() == 1);
    // Something outside the tag does nothing at all.
    PlayerProgress other;
    AdvancementTriggers again{advancements, other, told.hooks()};
    again.onInventoryChanged("Alex", "uuid-2", carrying({"minecraft:stone"}));
    CHECK(told.announced.size() == 1);
}

TEST_CASE("an advancement rewards experience and recipes when it completes") {
    Advancement advancement = wantingItems({ItemMatcher{.items = {Identifier::vanilla("stone")}}});
    advancement.rewards.experience = 100;
    advancement.rewards.recipes = {Identifier::vanilla("torch")};
    std::map<Identifier, Advancement> definitions;
    definitions.emplace(Identifier::vanilla("story/rewarded"), advancement);
    const RealAdvancements advancements{std::move(definitions)};
    PlayerProgress progress;
    Told told;
    AdvancementTriggers triggers{advancements, progress, told.hooks()};

    triggers.onInventoryChanged("Steve", "uuid-1", carrying({"minecraft:stone"}));

    REQUIRE(told.experience.size() == 1);
    CHECK(told.experience[0] == 100);
    REQUIRE(told.recipes.size() == 1);
    CHECK(told.recipes[0] == Identifier::vanilla("torch"));
    CHECK(progress.knownRecipes("uuid-1").contains(Identifier::vanilla("torch")));
}

TEST_CASE("an advancement that asks not to be announced is not") {
    Advancement advancement = wantingItems({ItemMatcher{.items = {Identifier::vanilla("stone")}}});
    advancement.display = AdvancementDisplay{.title = Component::text("Stone Age"),
                                             .description = Component::text("Mine stone"),
                                             .type = "task",
                                             .announceToChat = false};
    std::map<Identifier, Advancement> definitions;
    definitions.emplace(Identifier::vanilla("story/quiet"), advancement);
    const RealAdvancements advancements{std::move(definitions)};
    PlayerProgress progress;
    Told told;
    AdvancementTriggers triggers{advancements, progress, told.hooks()};

    triggers.onInventoryChanged("Steve", "uuid-1", carrying({"minecraft:stone"}));

    CHECK(told.announced.empty());
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("story/quiet")).size() == 1);
}

TEST_CASE("learning a recipe completes the advancement waiting for it") {
    std::map<Identifier, Advancement> definitions;
    definitions.emplace(Identifier::vanilla("recipes/torch"), wantingRecipe(Identifier::vanilla("torch")));
    const RealAdvancements advancements{std::move(definitions)};
    PlayerProgress progress;
    Told told;
    AdvancementTriggers triggers{advancements, progress, told.hooks()};

    triggers.onRecipeUnlocked("Steve", "uuid-1", Identifier::vanilla("torch"));
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("recipes/torch")).size() == 1);

    // A recipe nobody is waiting for changes nothing.
    triggers.onRecipeUnlocked("Steve", "uuid-1", Identifier::vanilla("ladder"));
    CHECK(told.announced.size() == 1);
}

TEST_CASE("an empty inventory triggers nothing at all") {
    std::map<Identifier, Advancement> definitions;
    definitions.emplace(Identifier::vanilla("story/mine_stone"),
                        wantingItems({ItemMatcher{.items = {Identifier::vanilla("stone")}}}));
    const RealAdvancements advancements{std::move(definitions)};
    PlayerProgress progress;
    Told told;
    AdvancementTriggers triggers{advancements, progress, told.hooks()};

    triggers.onInventoryChanged("Steve", "uuid-1", {});
    triggers.onInventoryChanged("Steve", "uuid-1", carrying({}));

    CHECK(told.announced.empty());
}

TEST_CASE("a trigger this build cannot observe never fires") {
    Advancement advancement;
    advancement.criteria.emplace_back(
        "flew", AdvancementCriterion{.trigger = Identifier::vanilla("levitation"),
                                     .condition = CriterionCondition{.kind = ConditionKind::Unsupported}});
    advancement.requirements.groups = {{"flew"}};
    std::map<Identifier, Advancement> definitions;
    definitions.emplace(Identifier::vanilla("nether/levitate"), advancement);
    const RealAdvancements advancements{std::move(definitions)};
    PlayerProgress progress;
    Told told;
    AdvancementTriggers triggers{advancements, progress, told.hooks()};

    triggers.onInventoryChanged("Steve", "uuid-1", carrying({"minecraft:stone"}));
    triggers.onRecipeUnlocked("Steve", "uuid-1", Identifier::vanilla("torch"));

    CHECK(told.announced.empty());
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("nether/levitate")).empty());
}

namespace {

/// One criterion of one trigger, as the vanilla files write them.
[[nodiscard]] Advancement waitingFor(const std::string& name, const Identifier& trigger,
                                     const CriterionCondition& condition) {
    Advancement advancement;
    advancement.display = AdvancementDisplay{
        .title = Component::text(name), .description = Component::text(name), .type = "task", .announceToChat = true};
    advancement.criteria.emplace_back(name, AdvancementCriterion{.trigger = trigger, .condition = condition});
    advancement.requirements.groups = {{name}};
    return advancement;
}

} // namespace

namespace {

/// One criterion as the jar writes it, read by the same parser the server uses.
[[nodiscard]] Advancement parsed(std::string_view trigger, std::string_view conditions) {
    const std::string json = R"({"criteria":{"c":{"trigger":")" + std::string{trigger} + R"(","conditions":)" +
                             std::string{conditions} + R"(}},"requirements":[["c"]]})";
    const std::expected<Advancement, basalt::server::world::advancement::AdvancementParseError> advancement =
        basalt::server::world::advancement::advancementJson::parse(json);
    REQUIRE(advancement.has_value());
    return *advancement;
}

[[nodiscard]] EntityFacts entity(std::string_view type) {
    return EntityFacts{.type = Identifier::vanilla(std::string{type})};
}

} // namespace

TEST_CASE("a kill grants only the criterion whose entity it matches, tags and killing blow included") {
    std::map<Identifier, Advancement> definitions;
    definitions.emplace(Identifier::vanilla("adventure/kill_a_mob"),
                        parsed("minecraft:player_killed_entity", R"({"entity":{"type":"minecraft:entity_properties",
                               "entity":"this","predicate":{"minecraft:entity_type":"#minecraft:raiders"}}})"));
    definitions.emplace(
        Identifier::vanilla("adventure/sniper_duel"),
        parsed("minecraft:player_killed_entity", R"({"killing_blow":{"tags":[{"id":"minecraft:is_projectile",
                               "expected":true}]}})"));
    const RealAdvancements advancements{std::move(definitions)};
    PlayerProgress progress;
    Told told;
    AdvancementTriggers::Hooks hooks = told.hooks();
    hooks.entityTagged = [](const Identifier& type, std::string_view tag) {
        return tag == "minecraft:raiders" && type.path() == "pillager";
    };
    hooks.damageTagged = [](const Identifier& type, std::string_view tag) {
        return tag == "minecraft:is_projectile" && type.path() == "arrow";
    };
    AdvancementTriggers triggers{advancements, progress, hooks};

    triggers.fire("player_killed_entity", "Steve", "uuid-1",
                  TriggerEvent{.entities = {{"entity", entity("zombie")}},
                               .damage = TriggerDamage{.type = Identifier::vanilla("player_attack")}});
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("adventure/kill_a_mob")).empty());
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("adventure/sniper_duel")).empty());

    triggers.fire("player_killed_entity", "Steve", "uuid-1",
                  TriggerEvent{.entities = {{"entity", entity("pillager")}},
                               .damage = TriggerDamage{.type = Identifier::vanilla("arrow")}});
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("adventure/kill_a_mob")).size() == 1);
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("adventure/sniper_duel")).size() == 1);
}

TEST_CASE("the player's own predicate reads the context the hooks give") {
    std::map<Identifier, Advancement> definitions;
    definitions.emplace(Identifier::vanilla("adventure/walk_on_powder_snow_with_leather_boots"),
                        parsed("minecraft:location", R"({"player":{"type":"minecraft:entity_properties",
                               "entity":"this","predicate":{"minecraft:equipment":{"feet":{"items":"minecraft:leather_boots"}}}}})"));
    const RealAdvancements advancements{std::move(definitions)};
    PlayerProgress progress;
    Told told;
    bool booted = false;
    AdvancementTriggers::Hooks hooks = told.hooks();
    hooks.playerContext = [&booted](std::string_view) {
        EntityFacts player = entity("player");
        if (booted) {
            player.equipment.emplace(
                "feet", basalt::server::world::loot::EquippedItem{.item = Identifier::vanilla("leather_boots")});
        }
        return std::optional{basalt::server::world::loot::LootContext{.thisEntity = player}};
    };
    AdvancementTriggers triggers{advancements, progress, hooks};

    triggers.fire("location", "Steve", "uuid-1");
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("adventure/walk_on_powder_snow_with_leather_boots"))
              .empty());
    booted = true;
    triggers.fire("location", "Steve", "uuid-1");
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("adventure/walk_on_powder_snow_with_leather_boots"))
              .size() == 1);
}

TEST_CASE("the dimension, the chest and the bucket each grant their own") {
    std::map<Identifier, Advancement> definitions;
    definitions.emplace(Identifier::vanilla("nether/root"),
                        waitingFor("entered", Identifier::vanilla("changed_dimension"),
                                   CriterionCondition{.kind = ConditionKind::ChangedDimension,
                                                      .toDimension = Identifier::vanilla("the_nether")}));
    definitions.emplace(Identifier::vanilla("adventure/salvage_sherd"),
                        waitingFor("rolled", Identifier::vanilla("player_generates_container_loot"),
                                   CriterionCondition{.kind = ConditionKind::ContainerLoot,
                                                      .lootTable = Identifier::vanilla("chests/simple_dungeon")}));
    definitions.emplace(Identifier::vanilla("husbandry/tadpole_in_a_bucket"),
                        parsed("minecraft:filled_bucket", R"({"item":{"items":"minecraft:tadpole_bucket"}})"));
    const RealAdvancements advancements{std::move(definitions)};
    PlayerProgress progress;
    Told told;
    AdvancementTriggers triggers{advancements, progress, told.hooks()};

    triggers.onChangedDimension("Steve", "uuid-1", Identifier::vanilla("overworld"), Identifier::vanilla("the_end"));
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("nether/root")).empty());
    triggers.onChangedDimension("Steve", "uuid-1", Identifier::vanilla("overworld"), Identifier::vanilla("the_nether"));
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("nether/root")).size() == 1);

    triggers.onContainerLoot("Steve", "uuid-1", Identifier::vanilla("chests/simple_dungeon"));
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("adventure/salvage_sherd")).size() == 1);

    triggers.fire("filled_bucket", "Steve", "uuid-1",
                  TriggerEvent{.items = {{"item", TriggerItem{.item = Identifier::vanilla("water_bucket")}}}});
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("husbandry/tadpole_in_a_bucket")).empty());
    triggers.fire("filled_bucket", "Steve", "uuid-1",
                  TriggerEvent{.items = {{"item", TriggerItem{.item = Identifier::vanilla("tadpole_bucket")}}}});
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("husbandry/tadpole_in_a_bucket")).size() == 1);
}

TEST_CASE("the victims of one arrow each need a predicate of their own") {
    std::map<Identifier, Advancement> definitions;
    const std::string_view creeper = R"({"type":"minecraft:entity_properties","entity":"this",
                                        "predicate":{"minecraft:entity_type":"minecraft:creeper"}})";
    definitions.emplace(Identifier::vanilla("adventure/two_creepers"),
                        parsed("minecraft:killed_by_arrow",
                               R"({"victims":[)" + std::string{creeper} + "," + std::string{creeper} + "]}"));
    const RealAdvancements advancements{std::move(definitions)};
    PlayerProgress progress;
    Told told;
    AdvancementTriggers triggers{advancements, progress, told.hooks()};

    triggers.fire("killed_by_arrow", "Steve", "uuid-1", TriggerEvent{.victims = {entity("creeper"), entity("pig")}});
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("adventure/two_creepers")).empty());
    triggers.fire("killed_by_arrow", "Steve", "uuid-1",
                  TriggerEvent{.victims = {entity("creeper"), entity("creeper")}});
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("adventure/two_creepers")).size() == 1);
}
