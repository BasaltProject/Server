#pragma once

#include <basalt/server/runtime/world/StuckSpeed.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

inline constexpr std::int32_t kItemLifetimeTicks = 6000;

/// parity: what a player's own toss gets, so it does not immediately fly back into their inventory.
inline constexpr std::int32_t kDefaultPickupDelay = 40;
inline constexpr std::int32_t kSpawnedPickupDelay = 10;
/// parity: `ItemEntity.setExtendedLifetime`.
inline constexpr std::int32_t kUnlimitedLifetimeAge = -32768;

inline constexpr std::int32_t kItemHealth = 5;

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

    /// parity: `Entity.stuckSpeedMultiplier`.
    stuckSpeed::Multiplier stuck;

    /// parity: `Entity.fallDistance`, read by powder snow only; `makeStuckInBlock` zeroes it.
    double fallDistance = 0.0;

    std::int32_t pickupDelay = kDefaultPickupDelay;

    std::int32_t age = 0;

    /// parity: `ItemEntity.health`.
    std::int32_t health = kItemHealth;

    /// parity: `ItemEntity` holds a whole `ItemStack`.
    wire::nbt::NbtCompound components;

    /// parity: `ItemEntity.thrower`, who let go of it, for the criteria its pickup fires.
    std::optional<wire::common::Uuid> thrower;

    /// parity: `Entity.move`.
    double previousX = 0.0;
    double previousY = 0.0;
    double previousZ = 0.0;

    bool resting = false;

    /// parity: `Entity.onGround()`.
    bool onGround = false;
    /// parity: `ItemEntity.tick`.
    bool crossedBlock = false;

    bool countChanged = false;

    /// Plugin controls: who may take it, and whether the ground slows it (0 default, 1 never, 2 always).
    bool mobsMayPickUp = true;
    bool playersMayPickUp = true;
    std::int8_t frictionState = 0;

    [[nodiscard]] bool canBePickedUp() const noexcept { return pickupDelay <= 0; }

    [[nodiscard]] bool canBePickedUpByMob() const noexcept { return pickupDelay <= 0 && mobsMayPickUp; }

    [[nodiscard]] bool canBePickedUpByPlayer() const noexcept { return pickupDelay <= 0 && playersMayPickUp; }
};

} // namespace basalt::server::runtime::world
