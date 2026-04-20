#pragma once

#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: the ten `onProjectileHit` overrides.
namespace projectileHits {

/// parity: `TargetBlock.updateRedstoneOutput`.
inline constexpr std::int32_t kArrowHoldTicks = 20;
inline constexpr std::int32_t kThrownHoldTicks = 8;

/// parity: `TntBlock.onProjectileHit`.
using OnDestroyed = std::function<void(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z)>;

[[nodiscard]] std::optional<BlockChange> onHit(WorldState& world, std::int64_t gameTime, const entity::BlockHit& hit,
                                               bool byArrow, bool burning, bool tntExplodes,
                                               const OnDestroyed& destroyed = {});

/// parity: `TargetBlock.getRedstoneStrength`.
[[nodiscard]] std::int32_t signalFor(const entity::BlockHit& hit);

[[nodiscard]] std::optional<BlockChange> releaseTarget(WorldState& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z);

} // namespace projectileHits

} // namespace basalt::server::runtime::world
