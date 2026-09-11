#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {

/// parity: `ItemEntity` despawns after five minutes.
inline constexpr std::int32_t kItemLifetimeTicks = 6000;

/// parity: what a player's own toss gets, so it does not immediately fly back into their inventory.
inline constexpr std::int32_t kDefaultPickupDelay = 40;

/// One stack lying on the ground.
struct DroppedItem {
    std::int32_t entityId = 0;
    wire::common::Uuid uuid;
    wire::common::Identifier item{"minecraft", "stone"};
    std::int32_t count = 1;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double velocityX = 0.0;
    double velocityY = 0.0;
    double velocityZ = 0.0;

    /// Ticks before anyone may pick this up.
    std::int32_t pickupDelay = kDefaultPickupDelay;

    /// Ticks lived.
    std::int32_t age = 0;

    /// What the stack carries besides its id and count, as a shulker box carries its contents.
    ///
    /// parity: `ItemEntity` holds a whole `ItemStack`, components and all. Only what a block put
    /// there travels this way today, which is enough for a box picked up and put back down.
    wire::nbt::NbtCompound components;

    /// Where it was when this tick began.
    ///
    /// parity: vanilla presses a plate from inside `Entity.move`, so the count it takes is of who is
    /// on the plate **at that instant**: whoever has not taken their turn yet is still where they
    /// were. Four items landing together therefore read one and not four, until the plate's own turn
    /// ten ticks later reads all of them. Measured in `placas`. Never saved.
    double previousX = 0.0;
    double previousY = 0.0;
    double previousZ = 0.0;

    /// Whether this settled: once true the server stops sending movement for it, as vanilla does.
    bool resting = false;

    /// parity: `Entity.onGround()`, which is not the same as settled: an item sliding along the
    /// floor is on the ground and still moving. It is what a big dripleaf asks before it tips.
    /// Never saved.
    bool onGround = false;
    /// parity: `ItemEntity.tick`'s `moved`, which is not "is it moving" but "did it change block this
    /// turn": that is what picks the two-tick merge rate over the forty-tick one. Never saved.
    bool crossedBlock = false;

    /// Set when a merge changed the count, so whoever can see it is told the new stack.
    bool countChanged = false;

    [[nodiscard]] bool canBePickedUp() const noexcept { return pickupDelay <= 0; }
};

} // namespace basalt::server::runtime::world
