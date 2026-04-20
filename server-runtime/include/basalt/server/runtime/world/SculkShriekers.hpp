#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/WardenSpawnTracker.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `SculkShriekerBlock`, `SculkShriekerBlockEntity` and `WardenSpawnTracker`.
namespace sculkShriekers {

inline constexpr std::int32_t kShriekTicks = 90;
inline constexpr std::int32_t kMaxWarning = wardenSpawnTracker::kMaxWarning;
inline constexpr double kPlayerSearchRadius = 16.0;
inline constexpr double kWardenCheckHalf = 24.0;
inline constexpr double kDarknessRadius = 40.0;
inline constexpr std::int32_t kReplyScatter = 10;
inline constexpr std::int32_t kSpawnAttempts = 20;
inline constexpr std::int32_t kSpawnRangeXZ = 5;
inline constexpr std::int32_t kSpawnRangeY = 6;
inline constexpr std::int32_t kShriekEvent = 3007;
inline constexpr std::string_view kWarningKey = "warning_level";

[[nodiscard]] bool isShrieker(std::int32_t stateId);

[[nodiscard]] bool isShrieking(std::int32_t stateId);

[[nodiscard]] bool canSummon(std::int32_t stateId);

[[nodiscard]] std::optional<std::int32_t> withShrieking(std::int32_t stateId, bool shrieking);

[[nodiscard]] std::int32_t warningLevel(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

void setWarningLevel(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level);

/// parity: `SpawnUtil.trySpawnMob` with `ON_TOP_OF_COLLIDER`.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> spawnPosition(WorldState& world, std::int32_t x,
                                                                       std::int32_t y, std::int32_t z,
                                                                       random::RandomSource& random,
                                                                       std::int32_t& attempt);

} // namespace sculkShriekers

} // namespace basalt::server::runtime::world
