#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/Piglins.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <doctest.h>

using basalt::server::runtime::entity::ServerEntity;
using basalt::server::world::loot::ItemStack;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
using basalt::wire::nbt::NbtCompound;
namespace piglins = basalt::server::runtime::entity::piglins;

TEST_CASE("a piglin's eight slots fill matching stacks first and hand back what does not fit") {
    piglins::PiglinState state;
    const ItemStack nuggets{.item = Identifier::vanilla("gold_nugget"), .count = 60};
    CHECK_FALSE(piglins::addToInventory(state, nuggets).has_value());
    CHECK_FALSE(piglins::addToInventory(state, nuggets).has_value());
    REQUIRE(state.inventory[0].has_value());
    CHECK(state.inventory[0]->count == 64);
    CHECK(state.inventory[1]->count == 56);
    for (std::int32_t fill = 0; fill < 6; ++fill) {
        static_cast<void>(piglins::addToInventory(state, ItemStack{.item = Identifier::vanilla("dirt"), .count = 64}));
    }
    CHECK_FALSE(piglins::canAddToInventory(state, ItemStack{.item = Identifier::vanilla("stone"), .count = 1}));
    const std::optional<ItemStack> left =
        piglins::addToInventory(state, ItemStack{.item = Identifier::vanilla("gold_nugget"), .count = 10});
    REQUIRE(left.has_value());
    CHECK(left->count == 2);
}

TEST_CASE("a piglin's inventory and anger survive a save, with the anger's time left") {
    const basalt::server::registry::EntityTypeEntry* entry =
        basalt::server::registry::entityTypes::of(Identifier::vanilla("piglin"));
    REQUIRE(entry != nullptr);
    ServerEntity piglin{1, Uuid{1, 1}, entry->id, entry->protocolId, 0.0, 64.0, 0.0};
    piglins::PiglinState& state = piglins::stateOf(piglin);
    state.inventory[0] = ItemStack{.item = Identifier::vanilla("gold_nugget"), .count = 9};
    state.cannotHunt = true;
    state.lastTick = 1000;
    state.angryAt.setFor(Uuid{5, 6}, 1000, 600);
    NbtCompound tag;
    piglins::write(piglin, tag);

    ServerEntity loaded{2, Uuid{2, 2}, entry->id, entry->protocolId, 0.0, 64.0, 0.0};
    loaded.sourceNbt = tag;
    const piglins::PiglinState& read = piglins::stateOf(loaded);
    CHECK(read.cannotHunt);
    REQUIRE(read.inventory[0].has_value());
    CHECK(read.inventory[0]->count == 9);
    CHECK(read.angryAt.value == Uuid{5, 6});
    // Kept as the ticks left until the first turn settles it.
    CHECK(read.angryAt.until == -2 - 600);
}
