#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/ItemEnchantability.hpp>
#include <basalt/server/runtime/world/MobLocalDifficulty.hpp>
#include <basalt/server/runtime/world/SelectableSlots.hpp>
#include <basalt/server/runtime/world/SpawnPotential.hpp>

#include <doctest.h>

#include <cmath>
#include <limits>
#include <string>

using namespace basalt::server::runtime::world;

TEST_CASE("peaceful is nothing, and the rest climbs with the world's age") {
    // parity: `DifficultyInstance`: peaceful is zero whatever else is true of the world.
    CHECK(mobLocalDifficulty::effectiveDifficulty("peaceful", 10000000, 10000000, 1.0F) == 0.0F);
    CHECK(mobLocalDifficulty::specialMultiplier("peaceful", 10000000, 10000000, 1.0F) == 0.0F);

    // A brand new easy world: the base scale and nothing else.
    CHECK(mobLocalDifficulty::effectiveDifficulty("easy", 0, 0, 0.0F) == doctest::Approx(0.75F));
    CHECK(mobLocalDifficulty::effectiveDifficulty("normal", 0, 0, 0.0F) == doctest::Approx(1.5F));
    CHECK(mobLocalDifficulty::effectiveDifficulty("hard", 0, 0, 0.0F) == doctest::Approx(2.25F));
    // A word nobody recognizes is easy, not a harder world than the operator asked for.
    CHECK(mobLocalDifficulty::effectiveDifficulty("nightmare", 0, 0, 0.0F) == doctest::Approx(0.75F));

    // The world's own age is worth a quarter at most, and only after the first hour.
    CHECK(mobLocalDifficulty::effectiveDifficulty("hard", 72000, 0, 0.0F) == doctest::Approx(2.25F));
    CHECK(mobLocalDifficulty::effectiveDifficulty("hard", 1512000, 0, 0.0F) == doctest::Approx(3.0F));
    // And no further, however long the world runs.
    CHECK(mobLocalDifficulty::effectiveDifficulty("hard", 100000000, 0, 0.0F) == doctest::Approx(3.0F));

    // Time spent in one chunk counts for more on hard than on normal, and half as much on easy.
    CHECK(mobLocalDifficulty::effectiveDifficulty("hard", 0, 3600000, 0.0F) == doctest::Approx(5.25F));
    CHECK(mobLocalDifficulty::effectiveDifficulty("normal", 0, 3600000, 0.0F) == doctest::Approx(3.0F));
    CHECK(mobLocalDifficulty::effectiveDifficulty("easy", 0, 3600000, 0.0F) == doctest::Approx(1.125F));
}

TEST_CASE("a full moon is worth nothing on a young world") {
    // parity: the moon's share is clamped by the global bonus, which is zero before the first hour:
    // a full moon on the first night changes nothing at all.
    CHECK(mobLocalDifficulty::effectiveDifficulty("hard", 0, 0, 1.0F) ==
          mobLocalDifficulty::effectiveDifficulty("hard", 0, 0, 0.0F));
    // Once the world is old enough for a bonus, the moon can be worth up to that same bonus.
    const float dark = mobLocalDifficulty::effectiveDifficulty("hard", 1512000, 0, 0.0F);
    const float full = mobLocalDifficulty::effectiveDifficulty("hard", 1512000, 0, 1.0F);
    CHECK(full > dark);
    CHECK(full == doctest::Approx(dark + 3.0F * 0.25F));
}

TEST_CASE("the multiplier a mob finalizer asks for is a line between two and four") {
    CHECK(mobLocalDifficulty::specialMultiplier("easy", 0, 0, 0.0F) == 0.0F);
    // Below two it is nothing at all rather than a negative share.
    CHECK(mobLocalDifficulty::effectiveDifficulty("normal", 0, 1000000, 0.0F) < 2.0F);
    CHECK(mobLocalDifficulty::specialMultiplier("normal", 0, 1000000, 0.0F) == 0.0F);
    // In between it is the difficulty's own distance past two, halved.
    const float effective = mobLocalDifficulty::effectiveDifficulty("normal", 0, 3600000, 0.0F);
    CHECK(effective > 2.0F);
    CHECK(effective < 4.0F);
    CHECK(mobLocalDifficulty::specialMultiplier("normal", 0, 3600000, 0.0F) ==
          doctest::Approx((effective - 2.0F) / 2.0F));
    CHECK(mobLocalDifficulty::specialMultiplier("hard", 100000000, 100000000, 1.0F) == 1.0F);
    // And in between it is the straight line, not a step.
    const float middle = mobLocalDifficulty::specialMultiplier("hard", 0, 1200000, 0.0F);
    CHECK(middle > 0.0F);
    CHECK(middle < 1.0F);
}

TEST_CASE("a mob is repelled by what is already there") {
    SpawnPotential field;
    const MobSpawnCost cost{.energyBudget = 0.7, .charge = 0.15};

    // An empty field allows anything: nothing to be repelled by.
    CHECK(field.allows(0, 64, 0, cost));
    CHECK(field.energyChange(0, 64, 0, cost.charge) == 0.0);

    // parity: every existing charge contributes `charge / distance`, and the proposal multiplies
    // that field by its own charge.
    field.add(10, 64, 0, 0.7);
    CHECK(field.size() == 1);
    CHECK(field.energyChange(0, 64, 0, 0.15) == doctest::Approx(0.7 / 10.0 * 0.15));
    CHECK(field.allows(0, 64, 0, cost));

    // Close enough and it is refused.
    CHECK_FALSE(field.allows(10, 65, 0, MobSpawnCost{.energyBudget = 0.01, .charge = 1.0}));

    // Exactly on top of another one is infinitely expensive rather than a division by zero, and it
    // is expensive whichever way the charge points: a negative one would otherwise come out as
    // minus infinity and be allowed anywhere.
    CHECK(field.energyChange(10, 64, 0, 0.15) == std::numeric_limits<double>::infinity());
    CHECK(field.energyChange(10, 64, 0, -0.15) == std::numeric_limits<double>::infinity());
    CHECK_FALSE(field.allows(10, 64, 0, cost));

    // Exactly at the budget is allowed: vanilla's own comparison is not a strict one.
    const double exact = field.energyChange(0, 64, 0, 0.15);
    CHECK(field.allows(0, 64, 0, MobSpawnCost{.energyBudget = exact, .charge = 0.15}));
    CHECK_FALSE(field.allows(0, 64, 0, MobSpawnCost{.energyBudget = std::nextafter(exact, 0.0), .charge = 0.15}));

    // A charge of nothing costs nothing wherever it goes, and is not remembered either.
    CHECK(field.energyChange(10, 64, 0, 0.0) == 0.0);
    field.add(1, 64, 1, 0.0);
    CHECK(field.size() == 1);

    // Several charges add up.
    field.add(-10, 64, 0, 0.7);
    CHECK(field.energyChange(0, 64, 0, 0.15) == doctest::Approx(2.0 * 0.7 / 10.0 * 0.15));
}

TEST_CASE("which shelf slot a click landed on") {
    // parity: only the face the block looks out of answers at all.
    CHECK_FALSE(selectableSlots::hitSlot("north", "south", 0.5F, 0.5F, 0.5F, 2, 3).has_value());
    CHECK_FALSE(selectableSlots::hitSlot("north", "up", 0.5F, 0.5F, 0.5F, 2, 3).has_value());
    CHECK_FALSE(selectableSlots::hitSlot("up", "up", 0.5F, 0.5F, 0.5F, 2, 3).has_value());

    // A chiseled bookshelf: two rows of three, counted from the top left as the player sees it.
    CHECK(selectableSlots::hitSlot("south", "south", 0.1F, 0.9F, 0.5F, 2, 3) == 0);
    CHECK(selectableSlots::hitSlot("south", "south", 0.5F, 0.9F, 0.5F, 2, 3) == 1);
    CHECK(selectableSlots::hitSlot("south", "south", 0.9F, 0.9F, 0.5F, 2, 3) == 2);
    CHECK(selectableSlots::hitSlot("south", "south", 0.1F, 0.1F, 0.5F, 2, 3) == 3);
    CHECK(selectableSlots::hitSlot("south", "south", 0.9F, 0.1F, 0.5F, 2, 3) == 5);

    // Looking the other way mirrors it, so the left slot is still the player's left.
    CHECK(selectableSlots::hitSlot("north", "north", 0.9F, 0.9F, 0.5F, 2, 3) == 0);
    CHECK(selectableSlots::hitSlot("north", "north", 0.1F, 0.9F, 0.5F, 2, 3) == 2);
    // And so do the two that read the other axis.
    CHECK(selectableSlots::hitSlot("west", "west", 0.5F, 0.9F, 0.1F, 2, 3) == 0);
    CHECK(selectableSlots::hitSlot("east", "east", 0.5F, 0.9F, 0.9F, 2, 3) == 0);

    // A click on the very edge belongs to the slot beside it rather than to nothing.
    CHECK(selectableSlots::hitSlot("south", "south", 0.0F, 1.0F, 0.5F, 2, 3) == 0);
    CHECK(selectableSlots::hitSlot("south", "south", 1.0F, 0.0F, 0.5F, 2, 3) == 5);
    // Including one the client rounded outside the block, which is clamped rather than refused.
    CHECK(selectableSlots::hitSlot("south", "south", 1.2F, 1.2F, 0.5F, 2, 3) == 2);
    CHECK(selectableSlots::hitSlot("south", "south", -0.2F, -0.2F, 0.5F, 2, 3) == 3);

    // A single row of one is the whole face.
    CHECK(selectableSlots::hitSlot("south", "south", 0.5F, 0.5F, 0.5F, 1, 1) == 0);
}

TEST_CASE("what an item is worth to an enchanting table") {
    // parity: `ToolMaterial`'s own values, which is a different table from the armour's: a golden
    // pickaxe takes enchantments better than a golden helmet does.
    CHECK(itemEnchantability::valueOf("golden_pickaxe") == 22);
    CHECK(itemEnchantability::valueOf("golden_helmet") == 25);
    CHECK(itemEnchantability::valueOf("wooden_sword") == 15);
    CHECK(itemEnchantability::valueOf("stone_axe") == 5);
    CHECK(itemEnchantability::valueOf("copper_hoe") == 13);
    CHECK(itemEnchantability::valueOf("iron_shovel") == 14);
    CHECK(itemEnchantability::valueOf("diamond_spear") == 10);
    CHECK(itemEnchantability::valueOf("netherite_pickaxe") == 15);

    // parity: `ArmorMaterials`' own values.
    CHECK(itemEnchantability::valueOf("leather_boots") == 15);
    CHECK(itemEnchantability::valueOf("copper_chestplate") == 8);
    CHECK(itemEnchantability::valueOf("chainmail_leggings") == 12);
    CHECK(itemEnchantability::valueOf("iron_helmet") == 9);
    CHECK(itemEnchantability::valueOf("diamond_boots") == 10);
    CHECK(itemEnchantability::valueOf("netherite_chestplate") == 15);
    // The one armour whose name carries no material at all.
    CHECK(itemEnchantability::valueOf("turtle_helmet") == 9);

    // The handful that name a value themselves.
    CHECK(itemEnchantability::valueOf("book") == 1);
    CHECK(itemEnchantability::valueOf("bow") == 1);
    CHECK(itemEnchantability::valueOf("crossbow") == 1);
    CHECK(itemEnchantability::valueOf("fishing_rod") == 1);
    CHECK(itemEnchantability::valueOf("trident") == 1);
    CHECK(itemEnchantability::valueOf("mace") == 15);

    // Written either way, since a plugin will write both.
    CHECK(itemEnchantability::valueOf("minecraft:iron_sword") == 14);

    // And what cannot be enchanted answers nothing, rather than zero.
    CHECK_FALSE(itemEnchantability::valueOf("stone").has_value());
    CHECK_FALSE(itemEnchantability::valueOf("stick").has_value());
    // A material nobody has armour of, and a suffix on nothing.
    CHECK_FALSE(itemEnchantability::valueOf("emerald_helmet").has_value());
    CHECK_FALSE(itemEnchantability::valueOf("_sword").has_value());
    CHECK_FALSE(itemEnchantability::valueOf("").has_value());
}
