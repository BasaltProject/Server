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

/// Which of the two vanilla hooks is asking, because concrete powder answers them differently.
enum class Asked {
    /// parity: `FallingBlock.onPlace`, which books whatever arrives and asks nothing else.
    onPlace,
    /// parity: `FallingBlock.updateShape`. `ConcretePowderBlock` overrides it and its concrete
    /// branch returns without calling super, so powder that water has reached books nothing here.
    updateShape,
};

/// Asks for this position to be looked at in two ticks, if what stands there can fall at all.
void scheduleIfFalling(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, Asked asked);

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

/// Whether the state could stand at that square, which only `blockSurvival` can answer.
using CanSurvive = std::function<bool(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z)>;

/// How a falling block ended its fall: as a block again, or as the item it shattered into.
struct Landing {
    std::optional<BlockChange> placed;
    /// parity: `FallingBlockEntity.tick`'s `spawnAtLocation(level, block)`, the block's **own** item
    /// and not its loot table: a falling gravel that cannot be placed leaves gravel, never flint.
    std::optional<std::int32_t> shattered;
};

/// Told the square a falling block has just written itself into, while that block is still there.
/// parity: `FallingBlockEntity.tick` writes with `setBlock(pos, state, 3)` and only then calls
/// `Fallable.onLand`, which for concrete powder writes a second time. Two writes owe two neighbour
/// updates, and the first is what runs the powder's own `onPlace`.
using OnArrival = std::function<void(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z)>;

/// Moves one falling block a tick on, and lands it. parity: `FallingBlockEntity.tick` adds gravity,
/// scales by air drag and stops on the first block that will not give way. The square has to be
/// replaceable and the state able to stand in it, or the block shatters.
[[nodiscard]] Landing tick(WorldState& world, entity::ServerEntity& entity, const CanSurvive& canSurvive,
                           const OnArrival& onArrival = {});

} // namespace fallingBlocks

} // namespace basalt::server::runtime::world
