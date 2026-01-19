#pragma once

#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/WorldMutex.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// Dropped items in this world, and their physics.
///
/// Behind the same lock as blocks: a break spawning one and a nearby player picking one up are
/// exactly the same kind of race two players editing blocks are.
class DroppedItemPhysics {
public:
    /// parity: `ItemEntity.getDefaultGravity`, `Entity.getAirDrag` and `ItemEntity.tick`'s own
    /// `movement.multiply(1.0, -0.5, 1.0)` on landing.
    static constexpr double kGravity = 0.04;
    static constexpr double kAirDrag = 0.98;
    static constexpr double kBounceDamping = -0.5;

    /// parity: `mergeWithNeighbours` inflates the item's **box** half a block sideways and nothing
    /// up, and then asks the two boxes to touch. Two half widths of an eighth each ride on top of
    /// that half block, so the centres can be three quarters apart; comparing the centres against
    /// half a block merged too little, and the bench caught it in `sensor`.
    static constexpr double kMergeRange = 0.75;
    static constexpr double kMergeHeight = 0.25;
    static constexpr std::int32_t kMergeMovingInterval = 2;
    static constexpr std::int32_t kMergeRestingInterval = 40;

    /// The stack this build fills, the same one the inventory works in.
    static constexpr std::int32_t kMaxStack = 64;

    /// parity: `EntityType.ITEM`'s own box, a quarter of a block each way.
    static constexpr double kItemWidth = 0.25;
    static constexpr double kItemHeight = 0.25;

    /// parity: `Entity.getBlockBounciness`, which gives anything that is not alive four fifths.
    static constexpr float kItemBounceShare = 0.8F;

    /// parity: `getBlockPosBelowThatAffectsMyMovement`, which looks half a block under the feet.
    static constexpr double kFrictionProbeDepth = 0.5;

    /// parity: the 1.0E-5 horizontal speed under which `ItemEntity.tick` skips its own physics.
    static constexpr double kRestingSpeedSquared = 1.0e-5;

    /// And the vertical speed that counts as none, so a bounce is never mistaken for a rest.
    static constexpr double kRestingSpeed = 1.0e-3;

    DroppedItemPhysics(WorldMutex& mutex, entity::entityCollision::BlockAt blockAt)
        : mutex_(&mutex), blockAt_(std::move(blockAt)) {}

    /// Adds a newly spawned (or re-added, for a partial pickup) dropped item to this world.
    DroppedItem& spawnItem(DroppedItem dropped);

    /// Every dropped item in this world, as a snapshot safe to iterate outside the lock.
    [[nodiscard]] std::vector<DroppedItem> items() const;

    /// A snapshot of every dropped item within `radius` blocks of a point, nearest first.
    [[nodiscard]] std::vector<DroppedItem> nearbyItems(double x, double y, double z, double radius) const;

    /// Removes and answers the dropped item with this entity id, or nothing if it is already gone.
    [[nodiscard]] std::optional<DroppedItem> removeItem(std::int32_t entityId);

    /// Shoves one item along, which is what a piston does to whatever its blocks sweep over.
    /// Answers false when the item is already gone.
    bool shiftItem(std::int32_t entityId, double dx, double dy, double dz);

    /// One tick of item physics for the whole world, answering the entity ids that despawned. parity:
    /// `ItemEntity.tick`: gravity, the move against real block shapes, drag, ground friction, the
    /// half-damped bounce and the merge with a neighbouring stack. Floating on a fluid is a gap.
    ///
    /// Does no I/O and takes no other lock: it runs on the tick thread, which must never touch a socket.
    [[nodiscard]] std::vector<std::int32_t> tickItems(const sched::Region* region = nullptr);

private:
    void moveItem(DroppedItem& item);
    void mergeWithNeighbours(std::size_t index, std::vector<std::int32_t>& despawned,
                             std::vector<std::int32_t>& absorbed);

    WorldMutex* mutex_;
    entity::entityCollision::BlockAt blockAt_;
    std::vector<DroppedItem> droppedItems_;
};

} // namespace basalt::server::runtime::world
