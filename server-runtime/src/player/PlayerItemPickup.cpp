#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerItemPickup.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <optional>

namespace basalt::server::runtime::player {

namespace {

/// parity: `Player`'s own reach for what is on the floor.
constexpr double kPickupRadius = 1.5;

} // namespace

std::vector<PickedItem> pickupNearbyItems(
    PlayerSession& session, world::WorldState& world,
    const std::function<bool(const PlayerSession&, const std::string&, std::int32_t, std::int32_t)>& allowed) {
    std::vector<PickedItem> taken;
    if (session.gamemode == wire::protocol::play::Gamemode::Spectator) {
        return taken;
    }
    for (const world::DroppedItem& candidate : world.nearbyItems(session.x, session.y, session.z, kPickupRadius)) {
        // parity: a player's own toss cannot be picked straight back up (`ItemEntity.pickupDelay`).
        if (!candidate.canBePickedUp()) {
            continue;
        }
        std::optional<world::DroppedItem> removed = world.removeItem(candidate.entityId);
        if (!removed.has_value()) {
            continue;
        }
        if (allowed && !allowed(session, removed->item.toString(), removed->count, removed->entityId)) {
            static_cast<void>(world.spawnItem(*removed));
            continue;
        }
        const std::int32_t leftover = session.inventory.add(storage::StoredItemStack{
            .id = removed->item.toString(), .count = removed->count, .components = removed->components});
        const std::int32_t picked = removed->count - leftover;
        if (picked <= 0) {
            static_cast<void>(world.spawnItem(*removed));
            continue;
        }
        // parity: `ItemEntity.playerTouch` counts the whole stack it touched.
        session.stats.increment(storage::stats::pickedUp(removed->item.toString()), removed->count);
        PickedItem row{.item = *removed, .picked = picked, .leftover = leftover};
        if (leftover > 0) {
            removed->count = leftover;
            static_cast<void>(world.spawnItem(*removed));
        }
        taken.push_back(std::move(row));
    }
    return taken;
}

} // namespace basalt::server::runtime::player
