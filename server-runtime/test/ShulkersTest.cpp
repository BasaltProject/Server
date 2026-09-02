#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::loot::ItemStack;
namespace nbt = basalt::wire::nbt;

namespace {

[[nodiscard]] basalt::wire::common::Identifier item(const std::string& path) {
    return basalt::wire::common::Identifier::vanilla(path);
}

} // namespace

TEST_CASE("every dye has a box, and the plain one counts too") {
    CHECK(shulkers::blocks().size() == 17);
    CHECK(shulkers::isShulkerBox("shulker_box"));
    CHECK(shulkers::isShulkerBox("light_blue_shulker_box"));
    CHECK(shulkers::isShulkerBox("black_shulker_box"));
    CHECK_FALSE(shulkers::isShulkerBox("chest"));
    CHECK_FALSE(shulkers::isShulkerBox("shulker"));
    CHECK_FALSE(shulkers::isShulkerBox(""));
}

TEST_CASE("a box that holds something carries it out and back in") {
    nbt::NbtCompound data;
    blockInventory::Slots slots(shulkers::kSize);
    slots[3] = ItemStack{.item = item("diamond"), .count = 5};
    slots[26] = ItemStack{.item = item("stone"), .count = 64};
    blockInventory::write(data, slots);

    const std::optional<nbt::NbtCompound> carried = shulkers::contentsOf(data);
    REQUIRE(carried.has_value());

    nbt::NbtCompound placed;
    shulkers::restore(placed, *carried);
    const blockInventory::Slots back = blockInventory::read(placed, shulkers::kSize);

    REQUIRE(back.size() == 27);
    REQUIRE(back[3].has_value());
    CHECK(back[3]->item == item("diamond"));
    CHECK(back[3]->count == 5);
    REQUIRE(back[26].has_value());
    CHECK(back[26]->count == 64);
}

TEST_CASE("an empty box carries nothing at all") {
    nbt::NbtCompound data;
    blockInventory::write(data, blockInventory::Slots(shulkers::kSize));

    CHECK_FALSE(shulkers::contentsOf(data).has_value());
    // And so does one that was never written to.
    CHECK_FALSE(shulkers::contentsOf(nbt::NbtCompound{}).has_value());
}

TEST_CASE("a box carries twenty-seven slots and no more") {
    nbt::NbtCompound data;
    blockInventory::Slots slots(64);
    slots[30] = ItemStack{.item = item("stone"), .count = 1};
    blockInventory::write(data, slots);

    // Slot thirty is outside a shulker box, so nothing of it is carried.
    CHECK_FALSE(shulkers::contentsOf(data).has_value());
}
