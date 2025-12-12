#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>

#include <doctest.h>

#include <string>
#include <vector>

using basalt::server::runtime::player::PlayerInventory;
using basalt::server::storage::StoredInventory;
using basalt::server::storage::StoredItemStack;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
namespace player = basalt::server::runtime::player;
namespace reg = basalt::server::registry;

namespace {

[[nodiscard]] StoredItemStack stackOf(const std::string& id, std::int32_t count) {
    return StoredItemStack{.id = id, .count = count, .components = NbtCompound{}};
}

} // namespace

TEST_CASE("the menu order and the storage order are each other's inverse") {
    // parity: `InventoryMenu`'s own layout: result, the 2x2 grid, armour, storage, hotbar, offhand.
    CHECK_FALSE(PlayerInventory::storageSlotOf(0).has_value());
    CHECK_FALSE(PlayerInventory::storageSlotOf(4).has_value());
    CHECK(PlayerInventory::storageSlotOf(5) == PlayerInventory::kArmourStart);
    CHECK(PlayerInventory::storageSlotOf(8) == PlayerInventory::kArmourStart + 3);
    CHECK(PlayerInventory::storageSlotOf(9) == 0);
    CHECK(PlayerInventory::storageSlotOf(44) == 35);
    CHECK(PlayerInventory::storageSlotOf(45) == PlayerInventory::kOffhandSlot);

    for (std::int32_t menuSlot = 5; menuSlot < PlayerInventory::kMenuSlots; menuSlot++) {
        const std::optional<std::int32_t> storage = PlayerInventory::storageSlotOf(menuSlot);
        REQUIRE(storage.has_value());
        CHECK(PlayerInventory::menuSlotOf(*storage) == menuSlot);
    }
}

TEST_CASE("a picked-up stack fills the hotbar before the rows above it") {
    PlayerInventory inventory;

    CHECK(inventory.add(stackOf("minecraft:stone", 1)) == 0);

    // parity: `Inventory.add` starts at the hotbar, which is slot 27 in the stored order.
    REQUIRE(inventory.at(PlayerInventory::kHotbarStart).has_value());
    CHECK(inventory.at(PlayerInventory::kHotbarStart)->count == 1);
}

TEST_CASE("a picked-up stack goes into one that is already there") {
    PlayerInventory inventory;
    inventory.set(5, stackOf("minecraft:stone", 60));

    CHECK(inventory.add(stackOf("minecraft:stone", 2)) == 0);

    CHECK(inventory.at(5)->count == 62);
    CHECK_FALSE(inventory.at(PlayerInventory::kHotbarStart).has_value());
}

TEST_CASE("what does not fit in a stack spills into the next slot and is reported") {
    PlayerInventory inventory;
    inventory.set(5, stackOf("minecraft:stone", 60));

    CHECK(inventory.add(stackOf("minecraft:stone", 10)) == 0);

    CHECK(inventory.at(5)->count == 64);
    CHECK(inventory.at(PlayerInventory::kHotbarStart)->count == 6);
}

TEST_CASE("a full inventory hands back what would not fit") {
    PlayerInventory inventory;
    for (std::int32_t slot = 0; slot < PlayerInventory::kMainSlots; slot++) {
        inventory.set(slot, stackOf("minecraft:dirt", 64));
    }

    CHECK(inventory.add(stackOf("minecraft:stone", 5)) == 5);
    CHECK(inventory.add(stackOf("minecraft:dirt", 5)) == 5);
}

TEST_CASE("a stack carrying different components does not merge with a plain one") {
    PlayerInventory inventory;
    StoredItemStack named = stackOf("minecraft:stone", 1);
    named.components.putString("minecraft:custom_name", "a name");
    inventory.set(5, named);

    CHECK(inventory.add(stackOf("minecraft:stone", 1)) == 0);

    CHECK(inventory.at(5)->count == 1);
    CHECK(inventory.at(PlayerInventory::kHotbarStart)->count == 1);
}

TEST_CASE("more than a stack's worth at once is spread over slots") {
    PlayerInventory inventory;

    CHECK(inventory.add(stackOf("minecraft:stone", 100)) == 0);

    CHECK(inventory.at(PlayerInventory::kHotbarStart)->count == 64);
    CHECK(inventory.at(PlayerInventory::kHotbarStart + 1)->count == 36);
}

TEST_CASE("counting and taking reach every slot the inventory fills from") {
    PlayerInventory inventory;
    inventory.set(0, stackOf("minecraft:stone", 10));
    inventory.set(PlayerInventory::kHotbarStart, stackOf("minecraft:stone", 5));
    inventory.set(PlayerInventory::kOffhandSlot, stackOf("minecraft:stone", 100));

    // parity: the offhand is not one of the slots picking up and counting scan.
    CHECK(inventory.countOf("minecraft:stone") == 15);

    CHECK(inventory.take("minecraft:stone", 7) == 7);
    CHECK(inventory.countOf("minecraft:stone") == 8);
    CHECK(inventory.take("minecraft:stone", 100) == 8);
    CHECK(inventory.countOf("minecraft:stone") == 0);
    CHECK(inventory.at(PlayerInventory::kOffhandSlot)->count == 100);
}

TEST_CASE("taking from an empty inventory takes nothing") {
    PlayerInventory inventory;

    CHECK(inventory.take("minecraft:stone", 5) == 0);
}

TEST_CASE("finding an item scans the main inventory and nothing else") {
    PlayerInventory inventory;
    inventory.set(PlayerInventory::kOffhandSlot, stackOf("minecraft:torch", 1));
    CHECK_FALSE(inventory.indexOf("minecraft:torch").has_value());

    inventory.set(12, stackOf("minecraft:torch", 1));
    CHECK(inventory.indexOf("minecraft:torch") == 12);
}

TEST_CASE("a picked block goes to the first free hotbar slot from the one in hand") {
    PlayerInventory inventory;
    inventory.setHeldSlot(3);

    CHECK(inventory.suitableHotbarSlot() == 3);

    inventory.set(PlayerInventory::kHotbarStart + 3, stackOf("minecraft:stone", 1));
    CHECK(inventory.suitableHotbarSlot() == 4);

    // It wraps rather than stopping at the end of the bar.
    for (std::int32_t slot = 4; slot < PlayerInventory::kHotbarSize; slot++) {
        inventory.set(PlayerInventory::kHotbarStart + slot, stackOf("minecraft:stone", 1));
    }
    CHECK(inventory.suitableHotbarSlot() == 0);

    // With every slot full it falls back to the one in hand.
    for (std::int32_t slot = 0; slot < PlayerInventory::kHotbarSize; slot++) {
        inventory.set(PlayerInventory::kHotbarStart + slot, stackOf("minecraft:stone", 1));
    }
    CHECK(inventory.suitableHotbarSlot() == 3);
}

TEST_CASE("the held slot is clamped to the hotbar") {
    PlayerInventory inventory;

    inventory.setHeldSlot(-4);
    CHECK(inventory.heldSlot() == 0);

    inventory.setHeldSlot(40);
    CHECK(inventory.heldSlot() == PlayerInventory::kHotbarSize - 1);
}

TEST_CASE("what is in hand is the hotbar slot the player picked") {
    PlayerInventory inventory;
    inventory.setHeldSlot(2);
    inventory.set(PlayerInventory::kHotbarStart + 2, stackOf("minecraft:sword", 1));

    REQUIRE(inventory.heldItem().has_value());
    CHECK(inventory.heldItem()->id == "minecraft:sword");
}

TEST_CASE("the first free slot is the first of the storage rows") {
    PlayerInventory inventory;
    CHECK(inventory.firstFreeSlot() == 0);

    for (std::int32_t slot = 0; slot < 4; slot++) {
        inventory.set(slot, stackOf("minecraft:stone", 1));
    }
    CHECK(inventory.firstFreeSlot() == 4);

    for (std::int32_t slot = 0; slot < PlayerInventory::kMainSlots; slot++) {
        inventory.set(slot, stackOf("minecraft:stone", 1));
    }
    CHECK_FALSE(inventory.firstFreeSlot().has_value());
}

TEST_CASE("swapping moves two slots and nothing else") {
    PlayerInventory inventory;
    inventory.set(1, stackOf("minecraft:stone", 3));

    inventory.swap(1, 2);

    CHECK_FALSE(inventory.at(1).has_value());
    REQUIRE(inventory.at(2).has_value());
    CHECK(inventory.at(2)->count == 3);
}

TEST_CASE("a slot outside the inventory is neither read nor written") {
    PlayerInventory inventory;

    inventory.set(-1, stackOf("minecraft:stone", 1));
    inventory.set(400, stackOf("minecraft:stone", 1));

    CHECK_FALSE(inventory.at(-1).has_value());
    CHECK_FALSE(inventory.at(400).has_value());
    CHECK(inventory.countOf("minecraft:stone") == 0);
}

TEST_CASE("every write says the inventory changed, and only the reader clears it") {
    PlayerInventory inventory;
    CHECK(inventory.changed());

    inventory.clearChanged();
    CHECK_FALSE(inventory.changed());

    CHECK(inventory.add(stackOf("minecraft:stone", 1)) == 0);
    CHECK(inventory.changed());

    inventory.clearChanged();
    // An add that fits nowhere changes nothing, so it says nothing changed either.
    for (std::int32_t slot = 0; slot < PlayerInventory::kMainSlots; slot++) {
        inventory.set(slot, stackOf("minecraft:dirt", 64));
    }
    inventory.clearChanged();
    CHECK(inventory.add(stackOf("minecraft:stone", 1)) == 1);
    CHECK_FALSE(inventory.changed());
}

TEST_CASE("a saved inventory comes back slot for slot") {
    PlayerInventory inventory;
    inventory.set(3, stackOf("minecraft:stone", 7));
    inventory.set(PlayerInventory::kOffhandSlot, stackOf("minecraft:shield", 1));

    const StoredInventory saved = inventory.snapshot();
    REQUIRE(saved.slots.size() == StoredInventory::kPlayerSlots);

    PlayerInventory restored;
    restored.set(1, stackOf("minecraft:dirt", 1));
    restored.restore(saved);

    CHECK_FALSE(restored.at(1).has_value());
    CHECK(restored.at(3)->count == 7);
    CHECK(restored.at(PlayerInventory::kOffhandSlot)->id == "minecraft:shield");
}

TEST_CASE("a window is forty-six slots with the five it does not model left empty") {
    PlayerInventory inventory;
    inventory.set(0, stackOf("minecraft:stone", 1));

    const std::vector<std::optional<basalt::wire::protocol::play::WireItemStack>> window = inventory.toWire();

    REQUIRE(window.size() == PlayerInventory::kMenuSlots);
    for (std::int32_t menuSlot = 0; menuSlot < 5; menuSlot++) {
        CHECK_FALSE(window[static_cast<std::size_t>(menuSlot)].has_value());
    }
    REQUIRE(window[9].has_value());
    CHECK(window[9]->count == 1);
}

TEST_CASE("an item this build's registry does not know is left out rather than mistaken") {
    PlayerInventory inventory;
    inventory.set(0, stackOf("minecraft:not_an_item", 1));
    inventory.set(1, stackOf("not a valid identifier at all", 1));

    CHECK_FALSE(inventory.toWireSlot(0).has_value());
    CHECK_FALSE(inventory.toWireSlot(1).has_value());
    CHECK_FALSE(inventory.toWireStack(std::nullopt).has_value());
}

TEST_CASE("a stack goes out with the item id the registry gave it") {
    PlayerInventory inventory;
    inventory.set(0, stackOf("minecraft:stone", 2));

    const std::optional<basalt::wire::protocol::play::WireItemStack> wire = inventory.toWireSlot(0);

    REQUIRE(wire.has_value());
    CHECK(wire->itemId == reg::items::registry().byName(Identifier::vanilla("stone"))->protocolId);
    CHECK(wire->count == 2);
}

TEST_CASE("an enchantment reaches the client only once its registry has gone out") {
    StoredItemStack sword = stackOf("minecraft:diamond_sword", 1);
    NbtCompound levels;
    levels.putInt("minecraft:sharpness", 3);
    sword.components.set(std::string{player::kEnchantmentsComponent}, basalt::wire::nbt::NbtTag::ofCompound(levels));

    PlayerInventory unsent;
    unsent.set(0, sword);
    REQUIRE(unsent.toWireSlot(0).has_value());
    CHECK(unsent.toWireSlot(0)->components.empty());

    PlayerInventory sent{[](const Identifier& name) {
        return name.path() == "sharpness" ? std::optional<std::int32_t>{12} : std::nullopt;
    }};
    sent.set(0, sword);
    REQUIRE(sent.toWireSlot(0).has_value());
    REQUIRE(sent.toWireSlot(0)->components.size() == 1);
    CHECK(sent.toWireSlot(0)->enchantments().size() == 1);
}

TEST_CASE("the levels on a stack are read by name") {
    StoredItemStack sword = stackOf("minecraft:diamond_sword", 1);
    CHECK(player::enchantmentLevel(sword, Identifier::vanilla("sharpness")) == 0);
    CHECK(player::enchantmentLevels(sword).empty());

    NbtCompound levels;
    levels.putInt("minecraft:sharpness", 3);
    levels.putInt("minecraft:unbreaking", 2);
    sword.components.set(std::string{player::kEnchantmentsComponent}, basalt::wire::nbt::NbtTag::ofCompound(levels));

    CHECK(player::enchantmentLevel(sword, Identifier::vanilla("sharpness")) == 3);
    CHECK(player::enchantmentLevel(sword, Identifier::vanilla("looting")) == 0);
    CHECK(player::enchantmentLevels(sword).size() == 2);
}
