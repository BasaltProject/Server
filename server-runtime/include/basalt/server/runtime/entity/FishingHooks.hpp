#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// A fishing bobber: how it is cast and how a fish finds it.
///
/// parity: `FishingHook`. The part that asks the world (moving, water, loot) is the caller's; what
/// is here is the state machine and its rolls.
namespace fishingHooks {

/// parity: `DATA_HOOKED_ENTITY` and `DATA_BITING`.
inline constexpr std::uint8_t kHookedDataIndex = 8;
inline constexpr std::uint8_t kBitingDataIndex = 9;

/// parity: the minute a bobber may lie on the ground, and the 32 blocks it may be from its owner.
inline constexpr std::int32_t kMaxGroundLife = 1200;
inline constexpr double kMaxOwnerDistanceSquared = 1024.0;

enum class Phase : std::uint8_t { Flying, HookedInEntity, Bobbing };

struct HookState {
    std::int32_t ownerId = 0;
    Phase phase = Phase::Flying;
    std::int32_t life = 0;
    std::int32_t nibble = 0;
    std::int32_t timeUntilLured = 0;
    std::int32_t timeUntilHooked = 0;
    std::int32_t outOfWaterTime = 0;
    float fishAngle = 0.0F;
    bool openWater = true;
    bool biting = false;
    std::optional<std::int32_t> hookedIn;
    std::int32_t luck = 0;
    std::int32_t lureSpeed = 0;
    /// parity: `Entity.random`, a fresh stream per bobber.
    random::RandomSource random = random::RandomSource::legacy(0);
};

/// parity: the `FishingHook(Player, ...)` constructor: where the bobber starts and how it flies off.
void cast(ServerEntity& hook, HookState& state, double ownerX, double ownerEyeY, double ownerZ, float yaw, float pitch);

/// What a lure turn shows, and what it needs to see.
struct Surroundings {
    std::function<void(std::string_view particle, double x, double y, double z, std::int32_t count, double dx,
                       double dy, double dz, double speed)>
        particles;
    std::function<void(float volume, float pitch)> splash;
    /// Whether the block at the point is plain water. parity: `BlockState.is(Blocks.WATER)`.
    std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z)> isWater;
    bool rainingAbove = false;
    bool skyAbove = true;
};

/// parity: `FishingHook.catchingFish`, one turn of the fish coming, circling and biting.
void catchingFish(ServerEntity& hook, HookState& state, const Surroundings& around);

/// What one block is to open water. parity: `FishingHook.OpenWaterType`.
enum class WaterLayer : std::uint8_t { AboveWater, InsideWater, Invalid };

/// parity: `FishingHook.calculateOpenWater`, a five by five column checked from one below to two
/// above.
[[nodiscard]] bool
calculateOpenWater(std::int32_t x, std::int32_t y, std::int32_t z,
                   const std::function<WaterLayer(std::int32_t, std::int32_t, std::int32_t)>& layerAt);

/// parity: `Mth.nextInt` and `Mth.nextFloat`, which answer the low bound when the range is empty.
[[nodiscard]] std::int32_t nextIntBetween(random::RandomSource& random, std::int32_t low, std::int32_t high);

[[nodiscard]] float nextFloatBetween(random::RandomSource& random, float low, float high);

} // namespace fishingHooks

} // namespace basalt::server::runtime::entity
