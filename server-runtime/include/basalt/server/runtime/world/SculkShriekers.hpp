#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/WardenSpawnTracker.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

/// The sculk shrieker and the warning it keeps for the players who set it off.
///
/// parity: `SculkShriekerBlock`, `SculkShriekerBlockEntity` and `WardenSpawnTracker`.
namespace sculkShriekers {

/// parity: `SHRIEKING_TICKS`, how long one shriek lasts.
inline constexpr std::int32_t kShriekTicks = 90;
inline constexpr std::int32_t kMaxWarning = wardenSpawnTracker::kMaxWarning;
/// parity: `WardenSpawnTracker`'s sixteen blocks of players and its 48-block box for wardens.
inline constexpr double kPlayerSearchRadius = 16.0;
inline constexpr double kWardenCheckHalf = 24.0;
/// parity: `DARKNESS_RADIUS` and the reply's ten-block scatter.
inline constexpr double kDarknessRadius = 40.0;
inline constexpr std::int32_t kReplyScatter = 10;
/// parity: `trySummonWarden`'s twenty tries, five across and six up and down.
inline constexpr std::int32_t kSpawnAttempts = 20;
inline constexpr std::int32_t kSpawnRangeXZ = 5;
inline constexpr std::int32_t kSpawnRangeY = 6;
/// parity: `LevelEvent.PARTICLES_SCULK_SHRIEK`.
inline constexpr std::int32_t kShriekEvent = 3007;
inline constexpr std::string_view kWarningKey = "warning_level";

[[nodiscard]] bool isShrieker(std::int32_t stateId);

[[nodiscard]] bool isShrieking(std::int32_t stateId);

[[nodiscard]] bool canSummon(std::int32_t stateId);

[[nodiscard]] std::optional<std::int32_t> withShrieking(std::int32_t stateId, bool shrieking);

[[nodiscard]] std::int32_t warningLevel(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

void setWarningLevel(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level);

/// parity: `SpawnUtil.trySpawnMob` with `ON_TOP_OF_COLLIDER`, the square a warden rises in.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> spawnPosition(WorldState& world, std::int32_t x,
                                                                       std::int32_t y, std::int32_t z,
                                                                       random::RandomSource& random,
                                                                       std::int32_t& attempt);

} // namespace sculkShriekers

} // namespace basalt::server::runtime::world
