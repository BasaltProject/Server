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

/// What an arrow gives back when it is picked up. parity: `AbstractArrow.getPickupItem`, which
/// hands back the stack it was fired from; this build does not keep a tipped arrow's potion, so a
/// tipped one comes back plain rather than coming back wrong.
[[nodiscard]] std::string itemOf(const entity::ServerEntity& arrow) {
    return arrow.type().path() == "spectral_arrow" ? "minecraft:spectral_arrow" : "minecraft:arrow";
}

/// An arrow that is not moving at all, which is what being stuck looks like from outside.
[[nodiscard]] bool isResting(const entity::ServerEntity& arrow) {
    return arrow.velocityX == 0.0 && arrow.velocityY == 0.0 && arrow.velocityZ == 0.0;
}

[[nodiscard]] bool isArrow(const entity::ServerEntity& entity) {
    const std::string_view path = entity.type().path();
    return path == "arrow" || path == "spectral_arrow";
}

} // namespace

std::vector<PickedArrow> pickupNearbyArrows(PlayerSession& session, world::WorldState& world) {
    std::vector<PickedArrow> taken;
    for (entity::ServerEntity* candidate : world.entities()) {
        // parity: `playerTouch` only takes a stuck arrow. Stillness says stuck, not `onGround`, which
        // reads false for an arrow held by a wall.
        if (candidate == nullptr || candidate->removed || !isArrow(*candidate) || !isResting(*candidate)) {
            continue;
        }
        const double dx = candidate->x - session.x;
        const double dy = candidate->y - session.y;
        const double dz = candidate->z - session.z;
        if ((dx * dx) + (dy * dy) + (dz * dz) > kPickupRadius * kPickupRadius) {
            continue;
        }
        // parity: `tryPickup` in creative takes the arrow away and gives nothing: the bag is
        // already full of whatever it wants.
        const bool creative = session.gamemode == wire::protocol::play::Gamemode::Creative;
        if (!creative) {
            const std::int32_t leftover =
                session.inventory.add(storage::StoredItemStack{.id = itemOf(*candidate), .count = 1, .components = {}});
            if (leftover > 0) {
                continue;
            }
        }
        candidate->removed = true;
        taken.push_back(PickedArrow{.entityId = candidate->entityId()});
    }
    return taken;
}

} // namespace basalt::server::runtime::player
