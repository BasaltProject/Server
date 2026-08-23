#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/world/DroppedItemPhysics.hpp>
#include <basalt/server/runtime/world/FluidCurrents.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/StuckSpeed.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

/// parity: `CactusBlock.entityInside`.
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

constexpr double kFloatDepth = 0.1;
constexpr double kWaterFluidDrag = 0.99;
constexpr double kLavaFluidDrag = 0.95;
constexpr double kFloatLift = 5.0e-4;
constexpr double kFloatLiftBelow = 0.06;

/// parity: `ItemEntity.setFluidMovement`.
void floatIn(DroppedItem& item, double drag) {
    item.velocityX *= static_cast<double>(static_cast<float>(drag));
    item.velocityY += item.velocityY < static_cast<double>(static_cast<float>(kFloatLiftBelow))
                          ? static_cast<double>(static_cast<float>(kFloatLift))
                          : 0.0;
    item.velocityZ *= static_cast<double>(static_cast<float>(drag));
}

[[nodiscard]] double squaredDistance(const DroppedItem& item, double x, double y, double z) {
    const double dx = item.x - x;
    const double dy = item.y - y;
    const double dz = item.z - z;
    return dx * dx + dy * dy + dz * dz;
}

[[nodiscard]] bool sameComponents(const wire::nbt::NbtCompound& first, const wire::nbt::NbtCompound& second) {
    return first.size() == 0 && second.size() == 0;
}

/// parity: `PowderSnowBlock.NUM_BLOCKS_TO_FALL_INTO_BLOCK`.
constexpr double kSnowCatchesAfter = 2.5;
constexpr double kSnowLidHeight = 0.9;

/// parity: `PowderSnowBlock.getCollisionShape` seen by a dropped item.
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
    if (at->item.path().ends_with("shulker_box")) {
        destroyed_.push_back(*at);
    }
    droppedItems_.erase(at);
    return true;
}

std::vector<DroppedItem> DroppedItemPhysics::takeDestroyed() {
    const WorldWriteLock lock{*mutex_};
    return std::exchange(destroyed_, {});
}

bool DroppedItemPhysics::pushItem(const std::int32_t entityId, const double dx, const double dy, const double dz) {
    const WorldWriteLock lock{*mutex_};
    const auto at =
        std::ranges::find_if(droppedItems_, [entityId](const DroppedItem& item) { return item.entityId == entityId; });
    if (at == droppedItems_.end()) {
        return false;
    }
    at->velocityX += dx;
    at->velocityY += dy;
    at->velocityZ += dz;
    return true;
}

bool DroppedItemPhysics::setItemCount(const std::int32_t entityId, const std::int32_t count) {
    const WorldWriteLock lock{*mutex_};
    const auto at =
        std::ranges::find_if(droppedItems_, [entityId](const DroppedItem& item) { return item.entityId == entityId; });
    if (at == droppedItems_.end()) {
        return false;
    }
    at->count = count;
    at->countChanged = true;
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

std::vector<DroppedItem> DroppedItemPhysics::expiring(const sched::Region* region) const {
    const WorldReadLock lock{*mutex_};
    std::vector<DroppedItem> found;
    for (const DroppedItem& item : droppedItems_) {
        if (item.age != kUnlimitedLifetimeAge && item.age + 1 >= lifetimeTicks_ &&
            (region == nullptr || region->ownsPosition(item.x, item.z))) {
            found.push_back(item);
        }
    }
    return found;
}

void DroppedItemPhysics::restartAge(std::int32_t entityId) {
    const WorldWriteLock lock{*mutex_};
    const auto at =
        std::ranges::find_if(droppedItems_, [entityId](const DroppedItem& item) { return item.entityId == entityId; });
    if (at != droppedItems_.end()) {
        at->age = 0;
    }
}

std::vector<std::int32_t> DroppedItemPhysics::tickItems(const sched::Region* region) {
    std::unique_lock<WorldMutex> lock{*mutex_};
    std::vector<std::int32_t> despawned;
    for (std::size_t index = 0; index < droppedItems_.size();) {
        DroppedItem& item = droppedItems_[index];
        if (region != nullptr && !region->ownsPosition(item.x, item.z)) {
            ++index;
            continue;
        }
        item.previousX = item.x;
        item.previousY = item.y;
        item.previousZ = item.z;
        if (item.age != kUnlimitedLifetimeAge) {
            ++item.age;
        }
        if (item.pickupDelay > 0) {
            --item.pickupDelay;
        }
        if (item.age >= lifetimeTicks_) {
            despawned.push_back(item.entityId);
            droppedItems_.erase(droppedItems_.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }
        // parity: `ItemEntity.tick` lets the current in twice.
        carryByFluid(item);
        moveItem(item);
        carryByFluid(item);
        // parity: `applyEffectsFromBlocks` runs on every tick of an item.
        item.health -= cactusDamage(blockAt_, item);
        if (item.health <= 0) {
            if (item.item.path().ends_with("shulker_box")) {
                destroyed_.push_back(item);
            }
            despawned.push_back(item.entityId);
            droppedItems_.erase(droppedItems_.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }
        ++index;
    }

    // parity: `ItemEntity.mergeWithNeighbours`.
    std::vector<std::int32_t> absorbed;
    for (std::size_t index = 0; index < droppedItems_.size(); ++index) {
        const DroppedItem& item = droppedItems_[index];
        if (std::ranges::find(absorbed, item.entityId) != absorbed.end()) {
            continue;
        }
        const std::int32_t interval = item.crossedBlock ? kMergeMovingInterval : kMergeRestingInterval;
        if (item.age % interval != 0) {
            continue;
        }
        mergeWithNeighbours(index, despawned, absorbed, lock);
    }
    return despawned;
}

void DroppedItemPhysics::mergeWithNeighbours(std::size_t index, std::vector<std::int32_t>& despawned,
                                             std::vector<std::int32_t>& absorbed, std::unique_lock<WorldMutex>& lock) {
    const auto maxOf = [](const DroppedItem& item) {
        return registry::itemStacking::maxStackSize(item.item, item.components);
    };
    // parity: `ItemEntity.isMergable`: a full stack takes nothing.
    if (droppedItems_[index].count >= maxOf(droppedItems_[index])) {
        return;
    }
    std::pair<std::int32_t, std::int32_t> approved{-1, -1};
    for (std::size_t other = 0; other < droppedItems_.size();) {
        if (other == index) {
            ++other;
            continue;
        }
        DroppedItem& candidate = droppedItems_[other];
        DroppedItem& item = droppedItems_[index];
        // parity: `ItemEntity.tryToMerge`: same item, same components, and the two counts have to.
        if (candidate.item != item.item || candidate.count <= 0 ||
            !sameComponents(candidate.components, item.components) || item.count + candidate.count > maxOf(candidate) ||
            std::abs(candidate.x - item.x) > mergeRange_ || std::abs(candidate.z - item.z) > mergeRange_ ||
            std::abs(candidate.y - item.y) > kMergeHeight) {
            ++other;
            continue;
        }
        if (mergeGate_ && approved != std::pair{item.entityId, candidate.entityId} && mergeListened_ &&
            mergeListened_()) {
            const DroppedItem into = item;
            const DroppedItem from = candidate;
            lock.unlock();
            const bool allowed = mergeGate_(into, from);
            lock.lock();
            const auto intoAt = std::ranges::find_if(
                droppedItems_, [&into](const DroppedItem& each) { return each.entityId == into.entityId; });
            const auto fromAt = std::ranges::find_if(
                droppedItems_, [&from](const DroppedItem& each) { return each.entityId == from.entityId; });
            if (intoAt == droppedItems_.end() || fromAt == droppedItems_.end()) {
                return;
            }
            index = static_cast<std::size_t>(intoAt - droppedItems_.begin());
            other = static_cast<std::size_t>(fromAt - droppedItems_.begin());
            if (!allowed) {
                ++other;
                continue;
            }
            approved = {into.entityId, from.entityId};
            continue;
        }
        const std::int32_t moved = std::min(std::min(maxOf(item), kMergeCap) - item.count, candidate.count);
        item.count += moved;
        item.countChanged = true;
        // parity: the merged stack keeps the younger age and the longer pickup delay.
        item.age = std::min(item.age, candidate.age);
        item.pickupDelay = std::max(item.pickupDelay, candidate.pickupDelay);
        if (moved < candidate.count) {
            candidate.count -= moved;
            candidate.countChanged = true;
            ++other;
            continue;
        }
        despawned.push_back(candidate.entityId);
        absorbed.push_back(candidate.entityId);
        droppedItems_.erase(droppedItems_.begin() + static_cast<std::ptrdiff_t>(other));
        if (other < index) {
            --index;
        }
        if (droppedItems_[index].count >= maxOf(droppedItems_[index])) {
            return;
        }
    }
}

double DroppedItemPhysics::fluidHeight(const DroppedItem& item, FluidKind kind) const {
    constexpr double kDeflate = 0.001;
    const double half = (kItemWidth / 2.0) - kDeflate;
    const double minY = item.y + kDeflate;
    const double maxY = item.y + kItemHeight - kDeflate;
    double height = 0.0;
    for (std::int32_t x = wire::common::mth::floor(item.x - half);
         x < static_cast<std::int32_t>(std::ceil(item.x + half)); ++x) {
        for (std::int32_t y = wire::common::mth::floor(minY); y < static_cast<std::int32_t>(std::ceil(maxY)); ++y) {
            for (std::int32_t z = wire::common::mth::floor(item.z - half);
                 z < static_cast<std::int32_t>(std::ceil(item.z + half)); ++z) {
                const std::int32_t state = blockAt_(x, y, z).value_or(0);
                if (fluidFlow::kindOf(state) != kind) {
                    continue;
                }
                const double top =
                    y + static_cast<double>(fluidFlow::heightOf(state, blockAt_(x, y + 1, z).value_or(0)));
                if (top >= minY) {
                    height = std::max(height, top - minY);
                }
            }
        }
    }
    return height;
}

void DroppedItemPhysics::carryByFluid(DroppedItem& item) const {
    const entity::Aabb box = entity::Aabb::of(item.x, item.y, item.z, kItemWidth, kItemHeight);
    const fluidCurrents::Contact contact = fluidCurrents::touch(blockAt_, box, true, nullptr);
    std::array<double, 3> movement{item.velocityX, item.velocityY, item.velocityZ};
    fluidCurrents::pushAll(movement, contact, false, false);
    item.velocityX = movement[0];
    item.velocityY = movement[1];
    item.velocityZ = movement[2];
}

void DroppedItemPhysics::moveItem(DroppedItem& item) {
    // parity: `ItemEntity.tick`.
    if (fluidHeight(item, FluidKind::Water) > kFloatDepth) {
        floatIn(item, kWaterFluidDrag);
    } else if (fluidHeight(item, FluidKind::Lava) > kFloatDepth) {
        floatIn(item, kLavaFluidDrag);
    } else {
        item.velocityY -= kGravity;
    }

    // parity: `ItemEntity.tick` only moves an item that is standing still on one tick in four.
    const bool horizontallyStill =
        (item.velocityX * item.velocityX) + (item.velocityZ * item.velocityZ) <= kRestingSpeedSquared;
    if (item.onGround && horizontallyStill && (item.age + item.entityId) % kRestingMoveEvery != 0) {
        return;
    }

    // parity: `Entity.move` spends what caught it last tick on **this** move and then drops the.
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
    // parity: `ItemEntity.tick` compares the floors.
    item.crossedBlock = std::floor(wasX) != std::floor(item.x) || std::floor(wasY) != std::floor(item.y) ||
                        std::floor(wasZ) != std::floor(item.z);

    // parity: `getBlockPosBelowThatAffectsMyMovement`.
    const std::optional<std::int32_t> under =
        blockAt_(wire::common::mth::floor(item.x), wire::common::mth::floor(item.y - kFrictionProbeDepth),
                 wire::common::mth::floor(item.z));
    const std::int32_t underState = under.value_or(registry::blocks::air().stateId);

    // parity: `Entity.restituteMovementAfterCollisions` with no bounce.
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

    // parity: `Entity.checkFallDamage`.
    if (onGround) {
        item.fallDistance = 0.0;
    } else if (moved[1] < 0.0) {
        item.fallDistance -= moved[1];
    }

    // parity: `checkInsideBlocks`.
    if (const std::optional<stuckSpeed::Multiplier> caught =
            stuckSpeed::caughtBy(blockAt_, box.move(moved[0], moved[1], moved[2]), moved[0], moved[1], moved[2]);
        caught.has_value()) {
        item.stuck = *caught;
        // parity: `Entity.makeStuckInBlock` starts with `resetFallDistance`.
        item.fallDistance = 0.0;
    }

    // parity: `ItemEntity.tick`'s own drag: the air takes its share every tick, and the block.
    double horizontalDrag = kAirDrag;
    if (onGround && item.frictionState != 1) {
        horizontalDrag *= static_cast<double>(registry::blockMovement::frictionOf(underState));
    }
    item.velocityX *= horizontalDrag;
    item.velocityY *= kAirDrag;
    item.velocityZ *= kAirDrag;
    // parity: `if (onGround) { if (movement.y < 0.0) multiply(1, -0.5, 1); }`. It never fires.
    if (onGround && item.velocityY < 0.0) {
        item.velocityY *= kBounceDamping;
    }

    // parity: the check that lets vanilla skip a settled item's physics, plus the vertical half of.
    item.onGround = onGround;
    item.resting = onGround &&
                   item.velocityX * item.velocityX + item.velocityZ * item.velocityZ <= kRestingSpeedSquared &&
                   std::abs(item.velocityY) <= kRestingSpeed;
    if (item.resting) {
        item.velocityX = 0.0;
        item.velocityY = 0.0;
        item.velocityZ = 0.0;
    }
}

} // namespace basalt::server::runtime::world
