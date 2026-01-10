#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/world/DroppedItemPhysics.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

[[nodiscard]] double squaredDistance(const DroppedItem& item, double x, double y, double z) {
    const double dx = item.x - x;
    const double dy = item.y - y;
    const double dz = item.z - z;
    return dx * dx + dy * dy + dz * dz;
}

/// Two stacks only merge when they carry the same extras, which for now means neither carries any.
[[nodiscard]] bool sameComponents(const wire::nbt::NbtCompound& first, const wire::nbt::NbtCompound& second) {
    return first.size() == 0 && second.size() == 0;
}

} // namespace

DroppedItem& DroppedItemPhysics::spawnItem(DroppedItem dropped) {
    const WorldWriteLock lock{*mutex_};
    droppedItems_.push_back(std::move(dropped));
    return droppedItems_.back();
}

std::vector<DroppedItem> DroppedItemPhysics::items() const {
    const WorldWriteLock lock{*mutex_};
    return droppedItems_;
}

std::vector<DroppedItem> DroppedItemPhysics::nearbyItems(double x, double y, double z, double radius) const {
    const WorldWriteLock lock{*mutex_};
    const double radiusSquared = radius * radius;
    std::vector<DroppedItem> found;
    for (const DroppedItem& item : droppedItems_) {
        if (squaredDistance(item, x, y, z) <= radiusSquared) {
            found.push_back(item);
        }
    }
    // Nearest first, and stable so two at the same distance keep the order they were dropped in.
    std::ranges::stable_sort(found, [x, y, z](const DroppedItem& first, const DroppedItem& second) {
        return squaredDistance(first, x, y, z) < squaredDistance(second, x, y, z);
    });
    return found;
}

std::optional<DroppedItem> DroppedItemPhysics::removeItem(std::int32_t entityId) {
    const WorldWriteLock lock{*mutex_};
    const auto at =
        std::ranges::find_if(droppedItems_, [entityId](const DroppedItem& item) { return item.entityId == entityId; });
    if (at == droppedItems_.end()) {
        return std::nullopt;
    }
    DroppedItem taken = std::move(*at);
    droppedItems_.erase(at);
    return taken;
}

bool DroppedItemPhysics::shiftItem(std::int32_t entityId, double dx, double dy, double dz) {
    const WorldWriteLock lock{*mutex_};
    const auto at =
        std::ranges::find_if(droppedItems_, [entityId](const DroppedItem& item) { return item.entityId == entityId; });
    if (at == droppedItems_.end()) {
        return false;
    }
    at->x += dx;
    at->y += dy;
    at->z += dz;
    return true;
}

std::vector<std::int32_t> DroppedItemPhysics::tickItems(const sched::Region* region) {
    const WorldWriteLock lock{*mutex_};
    std::vector<std::int32_t> despawned;
    for (std::size_t index = 0; index < droppedItems_.size();) {
        DroppedItem& item = droppedItems_[index];
        // An item belongs to whoever owns the ground it is lying on. With the world in one piece
        // that is everybody, and this is always true.
        if (region != nullptr && !region->ownsPosition(item.x, item.z)) {
            ++index;
            continue;
        }
        ++item.age;
        if (item.pickupDelay > 0) {
            --item.pickupDelay;
        }
        if (item.age >= kItemLifetimeTicks) {
            despawned.push_back(item.entityId);
            droppedItems_.erase(droppedItems_.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }
        if (!item.resting) {
            moveItem(item);
        }
        ++index;
    }

    // parity: `ItemEntity.mergeWithNeighbours`, asked every other tick while an item is moving and
    // every two seconds once it has settled. Without it a broken tree leaves six stacks of one log
    // on the floor where vanilla leaves one.
    std::vector<std::int32_t> absorbed;
    for (std::size_t index = 0; index < droppedItems_.size(); ++index) {
        const DroppedItem& item = droppedItems_[index];
        // Everything absorbed is erased on the spot, so this only guards against a merge that
        // learns to leave its leftovers behind.
        if (std::ranges::find(absorbed, item.entityId) != absorbed.end()) {
            continue;
        }
        const std::int32_t interval = item.resting ? kMergeRestingInterval : kMergeMovingInterval;
        if (item.age % interval != 0) {
            continue;
        }
        mergeWithNeighbours(index, despawned, absorbed);
    }
    return despawned;
}

void DroppedItemPhysics::mergeWithNeighbours(std::size_t index, std::vector<std::int32_t>& despawned,
                                             std::vector<std::int32_t>& absorbed) {
    // A full stack takes nothing, which the fit check inside the loop would also decide.
    if (droppedItems_[index].count >= kMaxStack) {
        return;
    }
    for (std::size_t other = 0; other < droppedItems_.size();) {
        if (other == index) {
            ++other;
            continue;
        }
        const DroppedItem& candidate = droppedItems_[other];
        DroppedItem& item = droppedItems_[index];
        // parity: `ItemEntity.tryToMerge`: same item, same components, and the two counts have to
        // fit in one stack.
        if (candidate.item != item.item || candidate.count <= 0 ||
            !sameComponents(candidate.components, item.components) || item.count + candidate.count > kMaxStack ||
            std::abs(candidate.x - item.x) > kMergeRange || std::abs(candidate.z - item.z) > kMergeRange ||
            std::abs(candidate.y - item.y) > kMergeHeight) {
            ++other;
            continue;
        }
        item.count += candidate.count;
        item.countChanged = true;
        // parity: the merged stack keeps the younger age and the longer pickup delay.
        item.age = std::min(item.age, candidate.age);
        item.pickupDelay = std::max(item.pickupDelay, candidate.pickupDelay);
        despawned.push_back(candidate.entityId);
        absorbed.push_back(candidate.entityId);
        droppedItems_.erase(droppedItems_.begin() + static_cast<std::ptrdiff_t>(other));
        if (other < index) {
            --index;
        }
        if (droppedItems_[index].count >= kMaxStack) {
            return;
        }
    }
}

void DroppedItemPhysics::moveItem(DroppedItem& item) {
    // parity: `ItemEntity.getDefaultGravity`, applied before the move rather than after it.
    item.velocityY -= kGravity;

    const entity::Aabb box = entity::Aabb::of(item.x, item.y, item.z, kItemWidth, kItemHeight);
    const std::array<double, 3> moved =
        entity::entityCollision::collide(blockAt_, box, item.velocityX, item.velocityY, item.velocityZ);
    item.x += moved[0];
    item.y += moved[1];
    item.z += moved[2];

    // parity: `getBlockPosBelowThatAffectsMyMovement`, the block whose friction an item feels.
    const std::optional<std::int32_t> under =
        blockAt_(wire::common::mth::floor(item.x), wire::common::mth::floor(item.y - kFrictionProbeDepth),
                 wire::common::mth::floor(item.z));
    const std::int32_t underState = under.value_or(registry::blocks::air().stateId);

    // parity: `Entity.restituteMovementAfterCollisions` with no bounce: a stopped axis is zeroed.
    const bool onGround = moved[1] != item.velocityY && item.velocityY < 0.0;
    const float bounciness = onGround ? registry::blockMovement::bouncinessOf(underState) * kItemBounceShare : 0.0F;
    if (moved[0] != item.velocityX) {
        item.velocityX = 0.0;
    }
    if (moved[2] != item.velocityZ) {
        item.velocityZ = 0.0;
    }
    if (moved[1] != item.velocityY) {
        item.velocityY = bounciness > 0.0F ? -item.velocityY * static_cast<double>(bounciness) : 0.0;
    }

    // parity: `ItemEntity.tick`'s own drag: the air takes its share every tick, and the block
    // underneath takes its friction on top of that.
    double horizontalDrag = kAirDrag;
    if (onGround) {
        horizontalDrag *= static_cast<double>(registry::blockMovement::frictionOf(underState));
    }
    item.velocityX *= horizontalDrag;
    item.velocityY *= kAirDrag;
    item.velocityZ *= kAirDrag;
    // parity: `if (onGround) { if (movement.y < 0.0) multiply(1, -0.5, 1); }`. It never fires
    // here: landing has just replaced the downward speed with zero or with the bounce, which is
    // upward. It is kept because it is the line vanilla runs, and because the day something
    // lands with speed left over is the day it matters.
    if (onGround && item.velocityY < 0.0) {
        item.velocityY *= kBounceDamping;
    }

    // parity: the check that lets vanilla skip a settled item's physics, plus the vertical half of
    // it. Here it also stops the movement packets, which is what makes a floor covered in drops
    // free to look at; an item still bouncing off slime is not settled and keeps moving.
    item.resting = onGround &&
                   item.velocityX * item.velocityX + item.velocityZ * item.velocityZ <= kRestingSpeedSquared &&
                   std::abs(item.velocityY) <= kRestingSpeed;
    // Already zero by the arithmetic above wherever this is true; written out so that what a
    // settled item carries is a decision rather than a rounding result.
    if (item.resting) {
        item.velocityX = 0.0;
        item.velocityY = 0.0;
        item.velocityZ = 0.0;
    }
}

} // namespace basalt::server::runtime::world
