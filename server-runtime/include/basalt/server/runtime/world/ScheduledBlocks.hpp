#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `tick`.
namespace scheduledBlocks {

[[nodiscard]] const wire::common::Identifier& frostedIceTickType();

[[nodiscard]] const wire::common::Identifier& driedGhastTickType();

[[nodiscard]] const wire::common::Identifier& snifferEggTickType();

[[nodiscard]] const wire::common::Identifier& frogspawnTickType();

[[nodiscard]] const wire::common::Identifier& bubbleColumnTickType();

[[nodiscard]] const wire::common::Identifier& sugarCaneTickType();

/// parity: `FarmlandBlock.updateShape`.
void scheduleCoveredSoil(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `BaseCoralPlantTypeBlock.tryScheduleDieTick`.
void scheduleCoralDeath(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t seed);

/// parity: `FarmlandBlock.turnToDirt`.
[[nodiscard]] std::optional<std::int32_t> coveredSoil(WorldState& world, std::int32_t x, std::int32_t y,
                                                      std::int32_t z);

/// parity: `CoralBlock.tick`.
[[nodiscard]] std::optional<std::int32_t> coral(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `ScaffoldingBlock.updateShape`.
void scheduleScaffolding(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

struct ScaffoldingTurn {
    std::optional<std::int32_t> reshaped;
    bool falls = false;
    bool breaks = false;
};

/// parity: `ScaffoldingBlock.tick`, the turn the shape update booked.
[[nodiscard]] ScaffoldingTurn scaffoldingTick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `ScaffoldingBlock.getDistance`.
[[nodiscard]] std::optional<std::int32_t> scaffolding(WorldState& world, std::int32_t x, std::int32_t y,
                                                      std::int32_t z);

[[nodiscard]] bool scaffoldingHasFallen(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `FrostedIceBlock.tick`.
[[nodiscard]] std::optional<BlockChange> frostedIceHeard(WorldState& world, std::int32_t x, std::int32_t y,
                                                         std::int32_t z);

void frostedIceChain(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                     std::vector<BlockChange>& changes);

void frostedIce(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                std::vector<BlockChange>& changes, bool hasSkylight = true);

/// parity: `DriedGhastBlock.spawnGhastling`.
struct Hatchling {
    wire::common::Identifier type;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    bool baby = false;
    /// parity: a tadpole comes out `setPersistenceRequired`; the other two do not.
    bool persistent = false;
};

/// parity: `DriedGhastBlock.tick`.
void driedGhast(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t gameTime,
                std::vector<BlockChange>& changes, std::vector<Hatchling>& hatched);

/// parity: `SnifferEggBlock.onPlace`.
void scheduleHatch(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t seed);

/// parity: `FrostedIceBlock.onPlace`.
void scheduleFrostedIce(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                        random::RandomSource& random);

/// parity: `FrogspawnBlock.onPlace`, an hour or so away and never the same twice.
void scheduleFrogspawn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t seed);

using IsPaleOakLog = std::function<bool(std::int32_t stateId)>;

/// parity: `CreakingHeartBlock.updateShape`.
void scheduleCreakingHeart(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `CreakingHeartBlock.tick`.
[[nodiscard]] std::optional<std::int32_t> creakingHeartTick(WorldState& world, std::int32_t x, std::int32_t y,
                                                            std::int32_t z, const IsPaleOakLog& isPaleOakLog,
                                                            bool creakingActive);

/// parity: `CREAKING_ACTIVE`.
[[nodiscard]] bool creakingIsActive(std::int64_t timeOfDay, bool hasDayTimeline = true);

struct SpeleothemTurn {
    bool destroys = false;
    bool falls = false;
};

/// parity: `SpeleothemBlock.updateShape`.
void scheduleSpeleothem(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool supported);

/// parity: `SpeleothemBlock.spawnFallingStalactite`.
[[nodiscard]] bool isStalactiteAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] bool isStalactiteTipAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `SpeleothemBlock.tick`, the turn it booked.
[[nodiscard]] SpeleothemTurn speleothemTick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                            bool supported);

/// parity: `SugarCaneBlock.updateShape`.
[[nodiscard]] bool booksItsOwnBreak(std::string_view path);

/// `supported` is the answer `canSurvive` would give.
void scheduleBreakIfLoose(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool supported);

void breakIfStillLoose(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool supported,
                       std::vector<BlockChange>& changes);

/// parity: `FrogspawnBlock.tick`.
void frogspawn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
               std::vector<BlockChange>& changes, std::vector<Hatchling>& hatched);

/// parity: `SnifferEggBlock.tick`.
void snifferEgg(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                std::vector<BlockChange>& changes, std::vector<Hatchling>& hatched);

/// parity: `LiquidBlock.tryScheduleBubbleBlockColumn`.
void scheduleBubbleColumn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                          std::optional<registry::BlockFace> towards);

/// parity: `BubbleColumnBlock.updateColumn`.
void bubbleColumn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes,
                  const std::function<void(std::int32_t, std::int32_t, std::int32_t)>& written = {});

} // namespace scheduledBlocks

} // namespace basalt::server::runtime::world
