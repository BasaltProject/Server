#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::runtime::world {

/// The conduit's frame and reach, without the players and mobs it acts on.
///
/// parity: `ConduitBlockEntity`.
namespace conduits {

/// parity: the `gameTime % 40` its shape is looked at on.
inline constexpr std::int64_t kShapeInterval = 40;
/// parity: `MIN_ACTIVE_SIZE` and `MIN_KILL_SIZE`.
inline constexpr std::size_t kMinActiveSize = 16;
inline constexpr std::size_t kMinKillSize = 42;
/// parity: `KILL_RANGE`.
inline constexpr std::int32_t kKillRange = 8;
/// parity: the `CONDUIT_POWER` it hands out, thirteen seconds.
inline constexpr std::int32_t kEffectTicks = 260;
/// parity: the magic damage one attack deals.
inline constexpr float kAttackDamage = 4.0F;
inline constexpr std::int64_t kAmbientInterval = 80;

using Position = std::array<std::int32_t, 3>;

/// The frame blocks around a conduit, in the jar's order, or nothing when the 3x3x3 around it is not
/// all water. parity: `ConduitBlockEntity.updateShape`.
[[nodiscard]] std::vector<Position> frame(const std::function<bool(std::int32_t, std::int32_t, std::int32_t)>& isWater,
                                          const std::function<bool(std::int32_t, std::int32_t, std::int32_t)>& isFrame,
                                          std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `applyEffects`' range, sixteen blocks per seven frame blocks.
[[nodiscard]] constexpr std::int32_t effectRange(std::size_t frameBlocks) {
    return static_cast<std::int32_t>(frameBlocks) / 7 * 16;
}

/// parity: `BlockPos.closerThan`, between whole block positions.
[[nodiscard]] bool closerThan(const Position& from, const Position& to, double distance);

} // namespace conduits

} // namespace basalt::server::runtime::world
