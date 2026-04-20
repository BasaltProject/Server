#pragma once

#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `FallingBlock` over `FallingBlockEntity`.
namespace fallingBlocks {

inline constexpr std::int32_t kFallDelayTicks = 2;

[[nodiscard]] const wire::common::Identifier& entityType();

[[nodiscard]] bool falls(std::int32_t stateId);

/// parity: `FallingBlock.isFree`: air, fire, a fluid or anything that gives way.
[[nodiscard]] bool isFree(std::int32_t stateId);

enum class Asked {
    /// parity: `FallingBlock.onPlace`, which books whatever arrives and asks nothing else.
    onPlace,
    /// parity: `FallingBlock.updateShape`.
    updateShape,
};

void scheduleIfFalling(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, Asked asked);

/// parity: `FallingBlockEntity.fall`.
entity::ServerEntity* fallFrom(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                               const std::function<std::int32_t()>& allocateEntityId);

entity::ServerEntity* beginFalling(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                   const std::function<std::int32_t()>& allocateEntityId);

using CanSurvive = std::function<bool(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z)>;

struct Landing {
    std::optional<BlockChange> placed;
    /// parity: `FallingBlockEntity.tick`.
    std::optional<std::int32_t> shattered;
};

/// parity: `FallingBlockEntity.tick`.
using OnArrival = std::function<void(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z)>;

/// parity: `FallingBlockEntity.tick`.
[[nodiscard]] Landing tick(WorldState& world, entity::ServerEntity& entity, const CanSurvive& canSurvive,
                           const OnArrival& onArrival = {});

} // namespace fallingBlocks

} // namespace basalt::server::runtime::world
