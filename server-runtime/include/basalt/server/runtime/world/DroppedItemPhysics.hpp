#pragma once

#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/WorldMutex.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

enum class FluidKind : std::uint8_t;

/// Behind the same lock as blocks: a break spawning an item and a pickup race like two players editing blocks.
class DroppedItemPhysics {
public:
    /// parity: `ItemEntity.getDefaultGravity`.
    static constexpr double kGravity = 0.04;
    static constexpr double kAirDrag = 0.98;
    static constexpr double kBounceDamping = -0.5;

    /// parity: `mergeWithNeighbours` inflates the box half a block sideways.
    static constexpr double kMergeRange = 0.75;
    static constexpr double kMergeHeight = 0.25;
    static constexpr std::int32_t kMergeMovingInterval = 2;
    static constexpr std::int32_t kMergeRestingInterval = 40;

    static constexpr std::int32_t kMergeCap = 64;

    static constexpr double kItemWidth = 0.25;
    static constexpr double kItemHeight = 0.25;

    static constexpr float kItemBounceShare = 0.8F;

    static constexpr double kFrictionProbeDepth = 0.5;

    /// parity: the 1.0E-5 horizontal speed under which `ItemEntity.tick` skips its own physics.
    static constexpr double kRestingSpeedSquared = 1.0e-5;

    /// And the vertical speed that counts as none, so a bounce is never mistaken for a rest.
    static constexpr double kRestingSpeed = 1.0e-3;

    /// parity: `(tickCount + getId()) % 4 == 0`.
    static constexpr std::int32_t kRestingMoveEvery = 4;

    DroppedItemPhysics(WorldMutex& mutex, entity::entityCollision::BlockAt blockAt)
        : mutex_(&mutex), blockAt_(std::move(blockAt)) {}

    DroppedItem& spawnItem(DroppedItem dropped);

    /// A snapshot safe to iterate outside the lock.
    [[nodiscard]] std::vector<DroppedItem> items() const;

    [[nodiscard]] std::vector<DroppedItem> nearbyItems(double x, double y, double z, double radius) const;

    [[nodiscard]] std::optional<DroppedItem> removeItem(std::int32_t entityId);

    bool shiftItem(std::int32_t entityId, double dx, double dy, double dz);

    /// parity: `ItemEntity.hurtServer` takes damage off the five and discards at zero.
    bool hurtItem(std::int32_t entityId, std::int32_t amount);

    bool setItemCount(std::int32_t entityId, std::int32_t count);

    /// parity: `ItemEntity.tick`.
    [[nodiscard]] std::vector<std::int32_t> tickItems(const sched::Region* region = nullptr);

private:
    /// parity: `Entity.getFluidHeight`, how deep the box sits in that fluid, from its bottom.
    [[nodiscard]] double fluidHeight(const DroppedItem& item, FluidKind kind) const;

    /// parity: `updateFluidInteraction` on an item: the current it floats in carries it.
    void carryByFluid(DroppedItem& item) const;

    void moveItem(DroppedItem& item);
    void mergeWithNeighbours(std::size_t index, std::vector<std::int32_t>& despawned,
                             std::vector<std::int32_t>& absorbed);

    WorldMutex* mutex_;
    entity::entityCollision::BlockAt blockAt_;
    std::vector<DroppedItem> droppedItems_;
};

} // namespace basalt::server::runtime::world
