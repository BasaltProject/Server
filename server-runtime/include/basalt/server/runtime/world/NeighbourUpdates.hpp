#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/GolemPatterns.hpp>
#include <basalt/server/runtime/world/RedstoneReactors.hpp>
#include <basalt/server/runtime/world/Reshapes.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `Level.updateNeighborsAt`.
namespace neighbourUpdates {

/// parity: `max-chained-neighbor-updates`.
inline constexpr std::int32_t kMaxChainedNeighbourUpdates = 1000000;

using OnReshaped = std::function<void(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z)>;

using OnBroken = OnReshaped;

/// A hook; left unset, `Dispensers` answers it, which is vanilla.
using DispenserNeighbourChange =
    std::function<std::optional<BlockChange>(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z)>;

/// A hook; left unset, `Walls` answers it, which is vanilla.
using WallShape = std::function<std::optional<std::map<std::string, std::string>>(
    std::int32_t stateId, std::int32_t north, std::int32_t south, std::int32_t west, std::int32_t east,
    std::int32_t above)>;

/// A hook; left unset, `RedstoneWire` answers it, which is vanilla.
using WireReshape =
    std::function<std::optional<std::int32_t>(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                              std::int32_t stateId, std::optional<registry::BlockFace> towards)>;

/// A struct rather than a world object so the cascade is testable without one.
struct Context {
    WorldState& world;
    std::string_view worldName;
    std::int32_t maxChainedUpdates = kMaxChainedNeighbourUpdates;
    /// Nothing outside these bounds is broken, so a wrong pair only spares blocks; the defaults are the overworld's.
    std::int32_t minY = -64;
    std::int32_t maxY = 319;
    random::RandomSource& random;
    /// parity: the `water_evaporates` dimension attribute.
    bool evaporates = false;
    bool hasSkylight = true;
    /// parity: the `ultrawarm` dimension attribute, read by `LavaFluid.getTickDelay`.
    bool ultrawarm = false;

    /// parity: `LiquidBlock`.
    using WakeFluids = std::function<void(std::int32_t x, std::int32_t y, std::int32_t z)>;

    /// parity: `CarvedPumpkinBlock.trySpawnGolem`.
    using SummonGolem = std::function<void(golemPatterns::Golem kind, double x, double y, double z)>;

    DispenserNeighbourChange dispenser;
    WakeFluids wakeFluids;
    /// parity: `LiquidBlock.onPlace`.
    WakeFluids arrivedFluid;
    WallShape wallShape;
    WireReshape wireReshape;
    reshapes::GeyserReshape geyser;
    SummonGolem summonGolem;
    /// parity: `#minecraft:copper`.
    golemPatterns::IsCopper isCopper;
    /// Stamps the game events a reactor raises (a door swinging is a `BLOCK_OPEN`).
    std::int64_t gameTime = 0;
    /// parity: `tnt_explodes`.
    bool tntExplodes = true;
    /// parity: `LevelChunk.setBlockState`.
    bool railArrivalTells = true;
};

/// parity: the block's `updateShape` is `BlockInput.place`'s pre-shape.
enum class ArrivedShape : std::uint8_t {
    Reshape,
    AlreadyShaped,
    /// Nothing arrived: a command writing half a door, where reshaping would undo the write.
    NotPlaced,
};

/// `removed` has no default on purpose; `afterShapes` runs after the shapes and before anything reacts.
void afterChange(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                 std::optional<std::int32_t> removed, std::vector<BlockChange>& effects, const OnBroken& onBroken,
                 const OnReshaped& onReshaped = {}, ArrivedShape arrivedShape = ArrivedShape::Reshape,
                 const std::function<void()>& afterShapes = {});

/// parity: `moveBlocks`.
void reshapeOnly(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z, const OnBroken& onBroken,
                 const OnReshaped& onReshaped = {});

/// parity: `SculkSensorBlock.onPlace`.
void settleArrivedSignal(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                         std::optional<std::int32_t> removed = std::nullopt, const OnReshaped& reshaped = {});

/// parity: `CrossCollisionBlock.updateShape`.
[[nodiscard]] std::optional<std::int32_t> reshape(const Context& context, std::int32_t x, std::int32_t y,
                                                  std::int32_t z,
                                                  std::optional<registry::BlockFace> towards = std::nullopt);

/// parity: `updateShape`.
[[nodiscard]] std::optional<std::int32_t> connect(const Context& context, std::int32_t x, std::int32_t y,
                                                  std::int32_t z, std::int32_t stateId,
                                                  std::optional<registry::BlockFace> towards = std::nullopt);

/// parity: `LeavesBlock.updateDistance`.
[[nodiscard]] std::int32_t leafDistance(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `LeavesBlock.tick`.
[[nodiscard]] std::optional<BlockChange> leafTick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace neighbourUpdates

} // namespace basalt::server::runtime::world
