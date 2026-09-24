#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerArrowPickup.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <doctest.h>

#include <memory>
#include <string>

using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::player::pickupNearbyArrows;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::WorldState;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
using basalt::wire::protocol::play::Gamemode;

namespace {

[[nodiscard]] ServerEntity& arrowIn(WorldState& world, const std::int32_t entityId, const double x, const double y,
                                    const double z, const bool stuck) {
    const Identifier type = Identifier::vanilla("arrow");
    const basalt::server::registry::EntityTypeEntry* entry = basalt::server::registry::entityTypes::of(type);
    const std::int32_t protocolId = entry == nullptr ? 0 : entry->protocolId;
    auto arrow = std::make_unique<ServerEntity>(entityId, Uuid{1, static_cast<std::uint64_t>(entityId)}, type,
                                                protocolId, x, y, z);
    // A stuck arrow is one that has stopped: the projectile code zeroes the movement when it lands.
    arrow->velocityY = stuck ? 0.0 : -0.4;
    arrow->arrowPickup = ServerEntity::ArrowPickup::Allowed;
    return world.spawnEntity(std::move(arrow));
}

[[nodiscard]] std::int32_t arrowsHeld(const PlayerSession& player) {
    std::int32_t held = 0;
    for (std::int32_t slot = 0; slot < basalt::server::runtime::player::PlayerInventory::kMainSlots; slot++) {
        const std::optional<basalt::server::storage::StoredItemStack>& stack = player.inventory.at(slot);
        if (stack.has_value() && stack->id == "minecraft:arrow") {
            held += stack->count;
        }
    }
    return held;
}

} // namespace

TEST_CASE("a player picks up the arrow they are standing on, and only that one") {
    WorldState world;
    PlayerSession player{Uuid{2, 2}, "Steve", 1, PlayerConnection{}};
    player.worldName = "world";
    player.x = 8.5;
    player.y = 64.0;
    player.z = 8.5;

    ServerEntity& stuck = arrowIn(world, 10, 8.6, 64.05, 8.4, true);
    // parity: `playerTouch` answers for a stuck arrow only, and only within reach.
    ServerEntity& flying = arrowIn(world, 11, 8.5, 64.5, 8.5, false);
    ServerEntity& far = arrowIn(world, 12, 12.0, 64.0, 8.5, true);

    const std::vector<basalt::server::runtime::player::PickedArrow> taken = pickupNearbyArrows(player, world);
    REQUIRE(taken.size() == 1);
    CHECK(taken.front().entityId == 10);
    CHECK(stuck.removed);
    CHECK_FALSE(flying.removed);
    CHECK_FALSE(far.removed);
    CHECK(arrowsHeld(player) == 1);
}

TEST_CASE("a full bag leaves the arrow where it is, and creative takes it without keeping it") {
    WorldState world;
    PlayerSession player{Uuid{3, 3}, "Steve", 1, PlayerConnection{}};
    player.worldName = "world";
    player.x = 8.5;
    player.y = 64.0;
    player.z = 8.5;
    for (std::int32_t slot = 0; slot < basalt::server::runtime::player::PlayerInventory::kMainSlots; slot++) {
        player.inventory.set(slot, basalt::server::storage::StoredItemStack{.id = "minecraft:stone", .count = 64});
    }

    ServerEntity& stuck = arrowIn(world, 20, 8.5, 64.0, 8.5, true);
    CHECK(pickupNearbyArrows(player, world).empty());
    CHECK_FALSE(stuck.removed);

    // parity: `Inventory.add` in creative swallows what does not fit, so the arrow goes and nothing
    // is added to a bag that has no room anyway.
    player.gamemode = Gamemode::Creative;
    CHECK(pickupNearbyArrows(player, world).size() == 1);
    CHECK(stuck.removed);
    CHECK(arrowsHeld(player) == 0);
}

TEST_CASE("a mob's arrow stays put and a tipped one comes back with its potion") {
    WorldState world;
    PlayerSession player{Uuid{4, 4}, "Steve", 1, PlayerConnection{}};
    player.worldName = "world";
    player.x = 8.5;
    player.y = 64.0;
    player.z = 8.5;

    ServerEntity& skeletons = arrowIn(world, 30, 8.5, 64.0, 8.5, true);
    skeletons.arrowPickup = ServerEntity::ArrowPickup::Disallowed;
    CHECK(pickupNearbyArrows(player, world).empty());
    CHECK_FALSE(skeletons.removed);
    skeletons.removed = true;

    ServerEntity& tipped = arrowIn(world, 31, 8.5, 64.0, 8.5, true);
    tipped.heldItem = "tipped_arrow";
    basalt::wire::nbt::NbtCompound contents;
    contents.putString("potion", "minecraft:poison");
    tipped.heldItemComponents.set("minecraft:potion_contents", basalt::wire::nbt::NbtTag::ofCompound(contents));
    REQUIRE(pickupNearbyArrows(player, world).size() == 1);
    const std::optional<std::int32_t> slot = player.inventory.indexOf("minecraft:tipped_arrow");
    REQUIRE(slot.has_value());
    const basalt::wire::nbt::NbtCompound* held =
        player.inventory.at(*slot)->components.getCompound("minecraft:potion_contents");
    REQUIRE(held != nullptr);
    CHECK(held->getString("potion") == "minecraft:poison");
}
