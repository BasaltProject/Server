#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/world/DroppedItemPhysics.hpp>
#include <basalt/server/runtime/world/StuckSpeed.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

/// parity: `CactusBlock.entityInside`, one point of damage per square of cactus the box touches, and
/// `ItemEntity.hurtServer`, which spends the item's health without any invulnerable window: five
/// turns against a cactus and the stack is gone. Nothing else here spends that health, because fire
/// and lava go through the fire clock and the stack's own `damage_resistant` component.
[[nodiscard]] std::int32_t cactusDamage(const entity::entityCollision::BlockAt& blocks, const DroppedItem& item) {
    constexpr double kEdgeSlack = 1.0e-7;
    const double half = DroppedItemPhysics::kItemWidth / 2.0;
    std::int32_t hits = 0;
    for (std::int32_t bx = wire::common::mth::floor(item.x - half + kEdgeSlack);
         bx <= wire::common::mth::floor(item.x + half - kEdgeSlack); ++bx) {
        for (std::int32_t by = wire::common::mth::floor(item.y + kEdgeSlack);
             by <= wire::common::mth::floor(item.y + DroppedItemPhysics::kItemHeight - kEdgeSlack); ++by) {
            for (std::int32_t bz = wire::common::mth::floor(item.z - half + kEdgeSlack);
                 bz <= wire::common::mth::floor(item.z + half - kEdgeSlack); ++bz) {
                const std::optional<std::int32_t> here = blocks(bx, by, bz);
                if (!here.has_value()) {
                    continue;
                }
                const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*here);
                if (block != nullptr && block->path() == "cactus") {
                    ++hits;
                }
            }
        }
    }
    return hits;
}

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

/// parity: `PowderSnowBlock.NUM_BLOCKS_TO_FALL_INTO_BLOCK`, past which the snow is something to land
/// on, and `FALLING_COLLISION_SHAPE`, the lid that is nine tenths of a block high.
constexpr double kSnowCatchesAfter = 2.5;
constexpr double kSnowLidHeight = 0.9;

/// parity: `PowderSnowBlock.getCollisionShape` seen by a dropped item, which is neither a falling
/// block nor anything that can walk on powder snow: the lid while it is still falling fast, and
/// nothing at all once it has stopped.
[[nodiscard]] entity::entityCollision::ShapeFor snowLidFor(const DroppedItem& item) {
    if (item.fallDistance <= kSnowCatchesAfter) {
        return {};
    }
    return [](std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) -> std::optional<entity::Aabb> {
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
        if (block == nullptr || block->path() != "powder_snow") {
            return std::nullopt;
        }
        const double blockX = x;
        const double blockY = y;
        const double blockZ = z;
        return entity::Aabb{.minX = blockX,
                            .minY = blockY,
                            .minZ = blockZ,
                            .maxX = blockX + 1.0,
                            .maxY = blockY + kSnowLidHeight,
                            .maxZ = blockZ + 1.0};
    };
}

} // namespace

DroppedItem& DroppedItemPhysics::spawnItem(DroppedItem dropped) {
    const WorldWriteLock lock{*mutex_};
    // It has not moved yet, so where it was and where it is are the same square.
    dropped.previousX = dropped.x;
    dropped.previousY = dropped.y;
    dropped.previousZ = dropped.z;
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

bool DroppedItemPhysics::hurtItem(const std::int32_t entityId, const std::int32_t amount) {
    const WorldWriteLock lock{*mutex_};
    const auto at =
        std::ranges::find_if(droppedItems_, [entityId](const DroppedItem& item) { return item.entityId == entityId; });
    if (at == droppedItems_.end()) {
        return false;
    }
    at->health -= amount;
    if (at->health > 0) {
        return false;
    }
    droppedItems_.erase(at);
    return true;
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
        item.previousX = item.x;
        item.previousY = item.y;
        item.previousZ = item.z;
        ++item.age;
        if (item.pickupDelay > 0) {
            --item.pickupDelay;
        }
        if (item.age >= kItemLifetimeTicks) {
            despawned.push_back(item.entityId);
            droppedItems_.erase(droppedItems_.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }
        moveItem(item);
        // parity: `applyEffectsFromBlocks` runs on every tick of an item, on the resting branch as
        // much as on the moving one, so a stack lying against a cactus is pricked four times in
        // four ticks and not once.
        item.health -= cactusDamage(blockAt_, item);
        if (item.health <= 0) {
            despawned.push_back(item.entityId);
            droppedItems_.erase(droppedItems_.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
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
        const std::int32_t interval = item.crossedBlock ? kMergeMovingInterval : kMergeRestingInterval;
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
    // parity: `ItemEntity.getDefaultGravity`, applied before the move rather than after it, and on
    // every tick whether or not the move below happens.
    item.velocityY -= kGravity;

    // parity: `ItemEntity.tick` only moves an item that is standing still on one tick in four, and
    // the drag below is inside that same branch. It is not an optimisation to skip: an item resting
    // on the lid powder snow offers a falling thing is held up by a shape that is gone the moment it
    // stops falling, and it is this fourth tick that lets it through. Measured in `nievepolvo`.
    const bool horizontallyStill =
        (item.velocityX * item.velocityX) + (item.velocityZ * item.velocityZ) <= kRestingSpeedSquared;
    if (item.onGround && horizontallyStill && (item.age + item.entityId) % kRestingMoveEvery != 0) {
        return;
    }

    // parity: `Entity.move` spends what caught it last tick on **this** move and then drops the
    // speed to nothing. What holds the item is asked again after the move, not here.
    double wantX = item.velocityX;
    double wantY = item.velocityY;
    double wantZ = item.velocityZ;
    if (stuckSpeed::holds(item.stuck)) {
        wantX *= item.stuck.x;
        wantY *= item.stuck.y;
        wantZ *= item.stuck.z;
        item.stuck = stuckSpeed::Multiplier{};
        item.velocityX = 0.0;
        item.velocityY = 0.0;
        item.velocityZ = 0.0;
    }

    const entity::Aabb box = entity::Aabb::of(item.x, item.y, item.z, kItemWidth, kItemHeight);
    const std::array<double, 3> moved =
        entity::entityCollision::collide(blockAt_, box, wantX, wantY, wantZ, snowLidFor(item));
    const double wasX = item.x;
    const double wasY = item.y;
    const double wasZ = item.z;
    item.x += moved[0];
    item.y += moved[1];
    item.z += moved[2];
    // parity: `ItemEntity.tick` compares the floors, so drifting inside one square is not "moved".
    item.crossedBlock = std::floor(wasX) != std::floor(item.x) || std::floor(wasY) != std::floor(item.y) ||
                        std::floor(wasZ) != std::floor(item.z);

    // parity: `getBlockPosBelowThatAffectsMyMovement`, the block whose friction an item feels.
    const std::optional<std::int32_t> under =
        blockAt_(wire::common::mth::floor(item.x), wire::common::mth::floor(item.y - kFrictionProbeDepth),
                 wire::common::mth::floor(item.z));
    const std::int32_t underState = under.value_or(registry::blocks::air().stateId);

    // parity: `Entity.restituteMovementAfterCollisions` with no bounce: a stopped axis is zeroed.
    // It compares against what the move **asked** for, which a cobweb has already cut down.
    const bool onGround = moved[1] != wantY && wantY < 0.0;
    const float bounciness = onGround ? registry::blockMovement::bouncinessOf(underState) * kItemBounceShare : 0.0F;
    if (moved[0] != wantX) {
        item.velocityX = 0.0;
    }
    if (moved[2] != wantZ) {
        item.velocityZ = 0.0;
    }
    if (moved[1] != wantY) {
        item.velocityY = bounciness > 0.0F ? -item.velocityY * static_cast<double>(bounciness) : 0.0;
    }

    // parity: `Entity.checkFallDamage`, which runs inside the move and before `checkInsideBlocks`:
    // landing puts the count back to zero, and falling adds what the move actually managed.
    if (onGround) {
        item.fallDistance = 0.0;
    } else if (moved[1] < 0.0) {
        item.fallDistance -= moved[1];
    }

    // parity: `checkInsideBlocks`, which runs inside the move and leaves behind what the next one
    // gets multiplied by.
    if (const std::optional<stuckSpeed::Multiplier> caught =
            stuckSpeed::caughtBy(blockAt_, box.move(moved[0], moved[1], moved[2]), moved[0], moved[1], moved[2]);
        caught.has_value()) {
        item.stuck = *caught;
        // parity: `Entity.makeStuckInBlock` starts with `resetFallDistance`, which is what lets an
        // item that has just landed on powder snow sink into it on the very next tick.
        item.fallDistance = 0.0;
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
    item.onGround = onGround;
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
