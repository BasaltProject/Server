#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerArrowPickup.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cmath>

namespace basalt::server::runtime::player {

namespace {

/// parity: `Player`'s reach for what is on the floor, the same one an item is taken from.
constexpr double kPickupRadius = 1.5;

/// parity: `AbstractArrow.getPickupItem`: the stack it was fired from, potion and all.
[[nodiscard]] storage::StoredItemStack itemOf(const entity::ServerEntity& arrow) {
    const std::string fallback{arrow.type().path() == "arrow" ? "arrow" : std::string{arrow.type().path()}};
    return storage::StoredItemStack{
        .id = "minecraft:" + arrow.heldItem.value_or(fallback), .count = 1, .components = arrow.heldItemComponents};
}

[[nodiscard]] bool isResting(const entity::ServerEntity& arrow) {
    return arrow.velocityX == 0.0 && arrow.velocityY == 0.0 && arrow.velocityZ == 0.0;
}

[[nodiscard]] bool isArrow(const entity::ServerEntity& entity) {
    const std::string_view path = entity.type().path();
    return path == "arrow" || path == "spectral_arrow" || path == "trident";
}

} // namespace

std::vector<PickedArrow> pickupNearbyArrows(PlayerSession& session, world::WorldState& world) {
    std::vector<PickedArrow> taken;
    for (entity::ServerEntity* candidate : world.entities()) {
        // parity: `playerTouch`.
        if (candidate == nullptr || candidate->removed || !isArrow(*candidate) ||
            (!isResting(*candidate) && !candidate->noPhysics)) {
            continue;
        }
        // parity: `ThrownTrident.playerTouch`: only its thrower, or anybody for one nobody threw.
        const bool trident = candidate->type().path() == "trident";
        const bool owned = candidate->ownerId == session.entityId();
        if (trident && candidate->ownerId.has_value() && !owned) {
            continue;
        }
        const double dx = candidate->x - session.x;
        const double dy = candidate->y - session.y;
        const double dz = candidate->z - session.z;
        if ((dx * dx) + (dy * dy) + (dz * dz) > kPickupRadius * kPickupRadius) {
            continue;
        }
        // parity: `tryPickup`.
        const bool creative = session.gamemode == wire::protocol::play::Gamemode::Creative;
        switch (candidate->arrowPickup) {
        case entity::ServerEntity::ArrowPickup::Disallowed:
            // parity: `ThrownTrident.tryPickup`.
            if (!(trident && candidate->noPhysics && owned && session.inventory.add(itemOf(*candidate)) == 0)) {
                continue;
            }
            break;
        case entity::ServerEntity::ArrowPickup::CreativeOnly:
            if (!creative) {
                continue;
            }
            break;
        case entity::ServerEntity::ArrowPickup::Allowed:
            if (session.inventory.add(itemOf(*candidate)) > 0 && !creative) {
                continue;
            }
            break;
        }
        candidate->removed = true;
        taken.push_back(PickedArrow{.entityId = candidate->entityId()});
    }
    return taken;
}

} // namespace basalt::server::runtime::player
