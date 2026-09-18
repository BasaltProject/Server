#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/AdvancementTriggers.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

using basalt::server::runtime::world::AdvancementTriggers;
using basalt::server::runtime::world::PlayerProgress;
using basalt::server::runtime::world::RealAdvancements;
using basalt::server::storage::StoredItemStack;
using basalt::server::world::advancement::Advancement;
using basalt::server::world::advancement::AdvancementCriterion;
using basalt::server::world::advancement::AdvancementDisplay;
using basalt::server::world::advancement::ConditionKind;
using basalt::server::world::advancement::CriterionCondition;
using basalt::server::world::advancement::ItemMatcher;
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

TEST_CASE("a kill grants only the criterion whose entity it matches, tags included") {
    std::map<Identifier, Advancement> definitions;
    definitions.emplace(Identifier::vanilla("adventure/kill_a_mob"),
                        waitingFor("killed", Identifier::vanilla("player_killed_entity"),
                                   CriterionCondition{.kind = ConditionKind::KilledEntity}));
    definitions.emplace(
        Identifier::vanilla("adventure/voluntary_exile"),
        waitingFor("raider", Identifier::vanilla("player_killed_entity"),
                   CriterionCondition{.kind = ConditionKind::KilledEntity, .entityTypes = {"#minecraft:raiders"}}));
    const RealAdvancements advancements{std::move(definitions)};
    PlayerProgress progress;
    Told told;
    AdvancementTriggers::Hooks hooks = told.hooks();
    hooks.entityTagged = [](const Identifier& type, std::string_view tag) {
        return tag == "minecraft:raiders" && type.path() == "pillager";
    };
    AdvancementTriggers triggers{advancements, progress, hooks};

    triggers.onKilledEntity("Steve", "uuid-1", Identifier::vanilla("zombie"));
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("adventure/kill_a_mob")).size() == 1);
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("adventure/voluntary_exile")).empty());

    triggers.onKilledEntity("Steve", "uuid-1", Identifier::vanilla("pillager"));
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("adventure/voluntary_exile")).size() == 1);
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
    definitions.emplace(
        Identifier::vanilla("husbandry/tadpole_in_a_bucket"),
        waitingFor("filled", Identifier::vanilla("filled_bucket"),
                   CriterionCondition{.kind = ConditionKind::FilledBucket,
                                      .matchers = {ItemMatcher{.items = {Identifier::vanilla("tadpole_bucket")}}}}));
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

    triggers.onFilledBucket("Steve", "uuid-1", Identifier::vanilla("water_bucket"));
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("husbandry/tadpole_in_a_bucket")).empty());
    triggers.onFilledBucket("Steve", "uuid-1", Identifier::vanilla("tadpole_bucket"));
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("husbandry/tadpole_in_a_bucket")).size() == 1);
}

TEST_CASE("a newborn grants the criterion of its kind, and the pair matches either way round") {
    std::map<Identifier, Advancement> definitions;
    definitions.emplace(Identifier::vanilla("husbandry/breed_an_animal"),
                        waitingFor("bred", Identifier::vanilla("bred_animals"),
                                   CriterionCondition{.kind = ConditionKind::BredAnimals}));
    definitions.emplace(
        Identifier::vanilla("husbandry/bred_all_animals"),
        waitingFor("sheep", Identifier::vanilla("bred_animals"),
                   CriterionCondition{.kind = ConditionKind::BredAnimals, .entityTypes = {"minecraft:sheep"}}));
    // parity: the frog pair, whose tadpole is not an animal, so the trigger names the parents.
    definitions.emplace(Identifier::vanilla("husbandry/froglights"),
                        waitingFor("frogs", Identifier::vanilla("bred_animals"),
                                   CriterionCondition{.kind = ConditionKind::BredAnimals,
                                                      .parentTypes = {"minecraft:frog"},
                                                      .partnerTypes = {"minecraft:frog"}}));
    const RealAdvancements advancements{std::move(definitions)};
    PlayerProgress progress;
    Told told;
    AdvancementTriggers triggers{advancements, progress, told.hooks()};

    triggers.onBredAnimals("Steve", "uuid-1", Identifier::vanilla("cow"), Identifier::vanilla("cow"),
                           Identifier::vanilla("cow"));
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("husbandry/breed_an_animal")).size() == 1);
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("husbandry/bred_all_animals")).empty());
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("husbandry/froglights")).empty());

    triggers.onBredAnimals("Steve", "uuid-1", Identifier::vanilla("sheep"), Identifier::vanilla("sheep"),
                           Identifier::vanilla("sheep"));
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("husbandry/bred_all_animals")).size() == 1);

    triggers.onBredAnimals("Steve", "uuid-1", Identifier::vanilla("tadpole"), Identifier::vanilla("frog"),
                           Identifier::vanilla("frog"));
    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("husbandry/froglights")).size() == 1);
}
