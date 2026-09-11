#pragma once

#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {

/// Sand and gravel coming down.
///
/// parity: `FallingBlock` over `FallingBlockEntity`. A block that loses what was under it schedules
/// itself two ticks out, and that tick turns it into an entity that falls at vanilla's own rate and
/// becomes a block again where it lands.
namespace fallingBlocks {

/// parity: `FallingBlock.getDelayAfterPlace`.
inline constexpr std::int32_t kFallDelayTicks = 2;

/// The entity, and what a scheduled tick on one of these blocks is filed under.
[[nodiscard]] const wire::common::Identifier& entityType();

[[nodiscard]] bool falls(std::int32_t stateId);

/// parity: `FallingBlock.isFree`: air, fire, a fluid or anything that gives way.
[[nodiscard]] bool isFree(std::int32_t stateId);

/// Asks for this position to be looked at in two ticks, if what stands there can fall at all.
void scheduleIfFalling(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// Turns the block into an entity wherever it stands, with no questions asked.
///
/// parity: `FallingBlockEntity.fall`, which is what `ScaffoldingBlock.tick` calls straight:
/// scaffolding already out of reach falls whether or not there is anything under it.
entity::ServerEntity* fallFrom(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                               const std::function<std::int32_t()>& allocateEntityId);

/// Turns the block into an entity if it still has nothing under it. Answers it, or nothing if the
/// tick came due over something solid after all.
entity::ServerEntity* beginFalling(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                   const std::function<std::int32_t()>& allocateEntityId);

/// Moves one falling block a tick on, and lands it. Answers the block it became, if it landed.
///
/// parity: `FallingBlockEntity.tick`, which adds gravity, scales by the air drag and stops on the
/// first block that will not give way. What vanilla does with the leftover (an anvil that cannot be
/// placed drops as an item) is not here: this build only puts the block back.
[[nodiscard]] std::optional<BlockChange> tick(WorldState& world, entity::ServerEntity& entity);

} // namespace fallingBlocks

} // namespace basalt::server::runtime::world
