#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/world/BrewingStands.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::loot::ItemStack;
namespace registry = basalt::server::registry;
namespace nbt = basalt::wire::nbt;

namespace {

[[nodiscard]] basalt::wire::common::Identifier item(const std::string& path) {
    return basalt::wire::common::Identifier::vanilla(path);
}

[[nodiscard]] ItemStack stack(const std::string& path, std::int32_t count = 1) {
    return ItemStack{.item = item(path), .count = count};
}

/// A stand with water bottles in every slot and an ingredient that turns them.
///
/// A bottle of water is the `potion` item carrying the `water` potion: one with no potion at all
/// brews nothing, which is what an empty glass bottle is.
[[nodiscard]] brewingStands::State loaded(std::int32_t bottles = 3) {
    brewingStands::State state;
    for (std::int32_t slot = 0; slot < bottles; ++slot) {
        state.items[static_cast<std::size_t>(slot)] = stack("potion");
        state.potions[static_cast<std::size_t>(slot)] = "water";
    }
    state.items[brewingStands::kIngredientSlot] = stack("nether_wart");
    return state;
}

} // namespace

TEST_CASE("the powder is only eaten when the fuel has run out") {
    brewingStands::State state = loaded();
    state.items[brewingStands::kFuelSlot] = stack("blaze_powder", 2);

    CHECK(brewingStands::tick(state));
    CHECK(state.fuel == registry::brewing::kFuelUses - 1);
    CHECK(state.items[brewingStands::kFuelSlot]->count == 1);

    // With fuel still in the tank the powder is left alone.
    const std::int32_t left = state.fuel;
    state.brewTime = 0;
    CHECK(brewingStands::tick(state));
    CHECK(state.fuel == left - 1);
    CHECK(state.items[brewingStands::kFuelSlot]->count == 1);
}

TEST_CASE("the last of the powder leaves the slot empty") {
    brewingStands::State state;
    state.items[brewingStands::kFuelSlot] = stack("blaze_powder", 1);

    CHECK(brewingStands::tick(state));
    CHECK(state.fuel == registry::brewing::kFuelUses);
    CHECK_FALSE(state.items[brewingStands::kFuelSlot].has_value());
}

TEST_CASE("anything but blaze powder is not fuel") {
    brewingStands::State state = loaded();
    state.items[brewingStands::kFuelSlot] = stack("gunpowder", 4);

    CHECK_FALSE(brewingStands::tick(state));
    CHECK(state.fuel == 0);
    CHECK(state.items[brewingStands::kFuelSlot]->count == 4);
}

TEST_CASE("a brew takes four hundred ticks and one unit of fuel") {
    brewingStands::State state = loaded();
    state.fuel = 2;

    CHECK(brewingStands::tick(state));
    CHECK(state.brewTime == registry::brewing::kBrewTime);
    CHECK(state.fuel == 1);

    for (std::int32_t turn = 0; turn < registry::brewing::kBrewTime; ++turn) {
        CHECK(brewingStands::tick(state));
    }
    CHECK(state.brewTime == 0);

    // Every bottle turned, and the wart is gone.
    for (std::int32_t slot = 0; slot < brewingStands::kBottles; ++slot) {
        CHECK(state.potions[static_cast<std::size_t>(slot)] == "awkward");
    }
    CHECK_FALSE(state.items[brewingStands::kIngredientSlot].has_value());
}

TEST_CASE("the ingredient loses one and stays what it is") {
    brewingStands::State state = loaded();
    state.items[brewingStands::kIngredientSlot] = stack("nether_wart", 2);
    state.fuel = 2;

    REQUIRE(brewingStands::tick(state));
    for (std::int32_t turn = 0; turn < registry::brewing::kBrewTime; ++turn) {
        REQUIRE(brewingStands::tick(state));
    }

    REQUIRE(state.items[brewingStands::kIngredientSlot].has_value());
    CHECK(state.items[brewingStands::kIngredientSlot]->item == item("nether_wart"));
    CHECK(state.items[brewingStands::kIngredientSlot]->count == 1);
}

TEST_CASE("nothing happens until the very last tick of the brew") {
    brewingStands::State state = loaded();
    state.fuel = 2;
    REQUIRE(brewingStands::tick(state));

    for (std::int32_t turn = 0; turn < registry::brewing::kBrewTime - 1; ++turn) {
        REQUIRE(brewingStands::tick(state));
        CHECK(state.potions[0] == "water");
    }
    REQUIRE(brewingStands::tick(state));
    CHECK(state.potions[0] == "awkward");
}

TEST_CASE("one unit of fuel is a whole brew, and does not call for more powder") {
    brewingStands::State state = loaded();
    state.fuel = 1;
    state.items[brewingStands::kFuelSlot] = stack("blaze_powder", 3);

    CHECK(brewingStands::tick(state));
    CHECK(state.brewTime == registry::brewing::kBrewTime);
    CHECK(state.fuel == 0);
    // The powder is still there: it is only eaten once the tank is empty.
    CHECK(state.items[brewingStands::kFuelSlot]->count == 3);
}

TEST_CASE("the first bottle counts as much as the last") {
    brewingStands::State state;
    state.items[0] = stack("potion");
    state.potions[0] = "water";
    state.items[brewingStands::kIngredientSlot] = stack("nether_wart");

    CHECK(brewingStands::isBrewable(state));
}

TEST_CASE("a brew that stops matching is abandoned, not finished") {
    brewingStands::State state = loaded();
    state.fuel = 5;
    REQUIRE(brewingStands::tick(state));
    REQUIRE(state.brewTime == registry::brewing::kBrewTime);

    // The ingredient is taken back out halfway through.
    for (std::int32_t turn = 0; turn < 100; ++turn) {
        REQUIRE(brewingStands::tick(state));
    }
    state.items[brewingStands::kIngredientSlot] = std::nullopt;

    CHECK(brewingStands::tick(state));
    CHECK(state.brewTime == 0);
    for (std::int32_t slot = 0; slot < brewingStands::kBottles; ++slot) {
        CHECK(state.potions[static_cast<std::size_t>(slot)] == "water");
    }
}

TEST_CASE("a brew whose ingredient is swapped for another is abandoned") {
    brewingStands::State state = loaded();
    state.fuel = 5;
    REQUIRE(brewingStands::tick(state));
    // parity: `serverTick` compares the slot with the ingredient the brew started on.
    state.items[brewingStands::kIngredientSlot] = stack("sugar");
    CHECK(brewingStands::tick(state));
    CHECK(state.brewTime == 0);
}

TEST_CASE("a bottle with nothing in it brews nothing") {
    // parity: `PotionBrewing.mix` needs a potion to turn: an empty bottle is not water.
    brewingStands::State state;
    state.items[0] = stack("potion");
    state.items[brewingStands::kIngredientSlot] = stack("nether_wart");
    state.fuel = 5;

    CHECK_FALSE(brewingStands::isBrewable(state));
}

TEST_CASE("a stand with nothing to brew does nothing at all") {
    brewingStands::State empty;
    CHECK_FALSE(brewingStands::tick(empty));
    CHECK_FALSE(brewingStands::isBrewable(empty));

    // An ingredient with no bottles, and bottles with no ingredient.
    brewingStands::State lonely;
    lonely.items[brewingStands::kIngredientSlot] = stack("nether_wart");
    lonely.fuel = 5;
    CHECK_FALSE(brewingStands::isBrewable(lonely));
    CHECK_FALSE(brewingStands::tick(lonely));

    brewingStands::State bottles;
    bottles.items[0] = stack("potion");
    bottles.potions[0] = "water";
    bottles.fuel = 5;
    CHECK_FALSE(brewingStands::isBrewable(bottles));

    // And an ingredient nothing brews with.
    brewingStands::State wrong = loaded();
    wrong.items[brewingStands::kIngredientSlot] = stack("dirt");
    wrong.fuel = 5;
    CHECK_FALSE(registry::brewing::isIngredient(item("dirt")));
    CHECK_FALSE(brewingStands::isBrewable(wrong));
}

TEST_CASE("brewing needs fuel, however good the ingredient is") {
    brewingStands::State state = loaded();

    CHECK(brewingStands::isBrewable(state));
    CHECK_FALSE(brewingStands::tick(state));
    CHECK(state.brewTime == 0);
}

TEST_CASE("the block shows which bottle slots are taken") {
    brewingStands::State state = loaded(2);

    const std::array<bool, 3> bits = brewingStands::bottleBits(state);
    CHECK(bits[0]);
    CHECK(bits[1]);
    CHECK_FALSE(bits[2]);
}

TEST_CASE("a stand goes to disk and comes back the same") {
    brewingStands::State state = loaded();
    state.fuel = 7;
    state.brewTime = 123;
    state.totalBrewTime = 300;
    state.totalFuel = 11;
    state.speedMultiplier = 1.5F;
    state.ingredient = item("nether_wart");
    state.potions[0] = "awkward";
    // The middle bottle holds nothing, which has to survive the round trip as nothing.
    state.potions[1] = std::nullopt;
    state.potions[2] = "strength";
    state.items[brewingStands::kFuelSlot] = stack("blaze_powder", 3);
    nbt::NbtCompound data;

    brewingStands::write(state, data);
    const brewingStands::State back = brewingStands::read(data);

    CHECK(back.fuel == 7);
    CHECK(back.brewTime == 123);
    CHECK(back.totalBrewTime == 300);
    CHECK(back.totalFuel == 11);
    CHECK(back.speedMultiplier == 1.5F);
    CHECK(back.ingredient == item("nether_wart"));
    CHECK(back.potions[0] == "awkward");
    CHECK_FALSE(back.potions[1].has_value());
    CHECK(back.potions[2] == "strength");
    REQUIRE(back.items[brewingStands::kFuelSlot].has_value());
    CHECK(back.items[brewingStands::kFuelSlot]->count == 3);
    REQUIRE(back.items[brewingStands::kIngredientSlot].has_value());
    CHECK(back.items[brewingStands::kIngredientSlot]->item == item("nether_wart"));
}

TEST_CASE("a tag that says nothing about the fire reads as cold") {
    nbt::NbtCompound bare;
    const brewingStands::State state = brewingStands::read(bare);

    CHECK(state.fuel == 0);
    CHECK(state.brewTime == 0);
    // parity: `getIntOr("total_brew_time", 400)` and `getIntOr("total_fuel", 20)`.
    CHECK(state.totalBrewTime == registry::brewing::kBrewTime);
    CHECK(state.totalFuel == registry::brewing::kFuelUses);
    CHECK(state.items.size() == brewingStands::kSize);
}

TEST_CASE("a potion taken out of a bottle is taken out of the tag too") {
    brewingStands::State state;
    state.potions[1] = "swiftness";
    nbt::NbtCompound data;
    brewingStands::write(state, data);
    REQUIRE(brewingStands::read(data).potions[1] == "swiftness");

    state.potions[1] = std::nullopt;
    brewingStands::write(state, data);

    CHECK_FALSE(brewingStands::read(data).potions[1].has_value());
}

TEST_CASE("with the jar's recipes a stand brews only what they list") {
    namespace crafting = basalt::server::world::crafting;
    // parity: 26.3 `potion_<potion>_gunpowder` recipes exist per potion, and none for `luck`.
    ItemStack splash = stack("splash_potion");
    nbt::NbtCompound contents;
    contents.putString("potion", "minecraft:water");
    splash.components.set("minecraft:potion_contents", nbt::NbtTag::ofCompound(std::move(contents)));
    crafting::Recipe recipe{.kind = crafting::RecipeKind::Brewing, .result = splash};
    recipe.base.items = {"minecraft:potion"};
    recipe.ingredient.items = {"minecraft:gunpowder"};
    recipe.potions = {"minecraft:water"};
    brewingStands::useRecipes({recipe});

    brewingStands::State state = loaded(2);
    state.potions[1] = "luck";
    state.items[brewingStands::kIngredientSlot] = stack("gunpowder");
    state.items[brewingStands::kFuelSlot] = stack("blaze_powder");
    CHECK(brewingStands::isReagent(item("gunpowder")));
    CHECK_FALSE(brewingStands::isReagent(item("nether_wart")));
    REQUIRE(brewingStands::tick(state));
    while (state.brewTime > 0) {
        brewingStands::tick(state);
    }
    CHECK(state.items[0]->item == item("splash_potion"));
    CHECK(state.potions[0] == "water");
    CHECK(state.items[1]->item == item("potion"));
    CHECK(state.potions[1] == "luck");

    brewingStands::useRecipes({});
}

TEST_CASE("a brewing recipe's potion tag matches what the tag holds") {
    namespace crafting = basalt::server::world::crafting;
    crafting::Recipe recipe{.kind = crafting::RecipeKind::Brewing, .result = stack("splash_potion")};
    recipe.base.items = {"minecraft:potion"};
    recipe.ingredient.items = {"minecraft:gunpowder"};
    recipe.potions = {"#minecraft:boom"};
    brewingStands::useRecipes({recipe}, [](const basalt::wire::common::Identifier& tag) {
        // parity: `PotionsPredicate.matches` asks the holder set the tag file bound.
        return tag.path() == "boom" ? std::vector<basalt::wire::common::Identifier>{item("water")}
                                    : std::vector<basalt::wire::common::Identifier>{};
    });

    brewingStands::State state = loaded(1);
    state.items[brewingStands::kIngredientSlot] = stack("gunpowder");
    state.fuel = 2;
    REQUIRE(brewingStands::isBrewable(state));
    REQUIRE(brewingStands::tick(state));
    while (state.brewTime > 0) {
        brewingStands::tick(state);
    }
    CHECK(state.items[0]->item == item("splash_potion"));

    // A tag no datapack defines binds to nothing, like vanilla's unbound holder set.
    brewingStands::useRecipes({recipe});
    brewingStands::State bare = loaded(1);
    bare.items[brewingStands::kIngredientSlot] = stack("gunpowder");
    CHECK_FALSE(brewingStands::isBrewable(bare));

    brewingStands::useRecipes({});
}
