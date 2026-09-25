#include <basalt/server/runtime/FireworkSystem.hpp>
#include <basalt/server/runtime/player/PlayerFireworks.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <array>

namespace basalt::server::runtime::player {
namespace {

/// parity: `FireworkRocketItem.ROCKET_PLACEMENT_OFFSET`.
constexpr double kPlacementOffset = 0.15;

[[nodiscard]] bool holdsRocket(const PlayerSession& player, std::int32_t slot) {
    const std::optional<storage::StoredItemStack>& held = player.inventory.at(slot);
    return held.has_value() && held->id == "minecraft:firework_rocket";
}

/// One rocket off the stack, as the entity carries it.
[[nodiscard]] wire::nbt::NbtCompound rocketOf(const storage::StoredItemStack& held) {
    wire::nbt::NbtCompound item;
    item.putString("id", held.id);
    item.putInt("count", 1);
    if (held.components.size() > 0) {
        item.set("components", wire::nbt::NbtTag::ofCompound(held.components));
    }
    return item;
}

/// parity: `ItemStack.consume`, which an infinite-materials player never pays.
void spendOne(const BreakContext& context, PlayerSession& player, std::int32_t slot) {
    if (player.gamemode == wire::protocol::play::Gamemode::Creative) {
        return;
    }
    std::optional<storage::StoredItemStack> held = player.inventory.at(slot);
    if (!held.has_value()) {
        return;
    }
    held->count -= 1;
    player.inventory.set(slot, held->count <= 0 ? std::nullopt : held);
    if (context.slotChanged) {
        context.slotChanged(player, slot);
    }
}

[[nodiscard]] std::array<std::int32_t, 3> stepOf(wire::protocol::play::Direction face) {
    switch (face) {
    case wire::protocol::play::Direction::Down:
        return {0, -1, 0};
    case wire::protocol::play::Direction::Up:
        return {0, 1, 0};
    case wire::protocol::play::Direction::North:
        return {0, 0, -1};
    case wire::protocol::play::Direction::South:
        return {0, 0, 1};
    case wire::protocol::play::Direction::West:
        return {-1, 0, 0};
    case wire::protocol::play::Direction::East:
        return {1, 0, 0};
    }
    return {0, 1, 0};
}

} // namespace

bool useRocketFor(const BreakContext& context, FireworkSystem& fireworks, PlayerSession& player,
                  world::LoadedWorld& world, std::int32_t slot) {
    if (!holdsRocket(player, slot) || !player.fallFlying) {
        return false;
    }
    static_cast<void>(
        fireworks.attach(world, rocketOf(*player.inventory.at(slot)), player.entityId(), player.x, player.y, player.z));
    spendOne(context, player, slot);
    return true;
}

bool useRocketOnFor(const BreakContext& context, FireworkSystem& fireworks, PlayerSession& player,
                    world::LoadedWorld& world, std::int32_t slot, const BlockHit& hit) {
    if (!holdsRocket(player, slot)) {
        return false;
    }
    // parity: a glider's click on a block passes, and the item's own use gets the turn instead.
    if (player.fallFlying) {
        return false;
    }
    const std::array<std::int32_t, 3> step = stepOf(hit.direction);
    static_cast<void>(fireworks.launch(world, rocketOf(*player.inventory.at(slot)),
                                       hit.x + static_cast<double>(hit.clickX) + (step[0] * kPlacementOffset),
                                       hit.y + static_cast<double>(hit.clickY) + (step[1] * kPlacementOffset),
                                       hit.z + static_cast<double>(hit.clickZ) + (step[2] * kPlacementOffset),
                                       player.entityId()));
    spendOne(context, player, slot);
    return true;
}

} // namespace basalt::server::runtime::player
