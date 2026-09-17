#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

/// The two fluids that flow, and the sixteen block states each of them owns.
enum class FluidKind : std::uint8_t { Water, Lava };

/// parity: `FlowingFluid.isSource`, which is the square holding a full amount and not flowing.
[[nodiscard]] bool isSourceFluid(std::int32_t stateId);

/// The name a fluid's scheduled turn is stored under.
///
/// parity: `LiquidBlock` books with `state.getFluidState().getType()`, and a square that is not a
/// source answers `FLOWING_WATER` or `FLOWING_LAVA` rather than the still fluid.
[[nodiscard]] const wire::common::Identifier& fluidId(FluidKind kind, bool source);

[[nodiscard]] std::int32_t baseStateId(FluidKind kind);

/// One block a flow rewrote, plus what it washed away so the caller can drop it.
struct FluidChange {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t stateId = 0;
    std::int32_t washedAway = 0;
};

/// Water and lava spreading, one scheduled tick at a time.
///
/// parity: `FlowingFluid`, with `WaterFluid`/`LavaFluid` for the constants and `LiquidBlock` for
/// when a tick is scheduled. A fluid moves only when `onBlockChanged` wakes it.
namespace fluidFlow {

/// parity: `WaterFluid.getTickDelay`, and `LavaFluid`'s, which is faster in the Nether.
inline constexpr std::int32_t kWaterTickDelay = 5;
inline constexpr std::int32_t kLavaTickDelay = 30;
inline constexpr std::int32_t kFastLavaTickDelay = 10;

/// Everything a flow needs besides the blocks: whether this dimension boils lava (the Nether's
/// `ultrawarm`), the two source-conversion rules, and the roll behind lava's spread delay.
struct Context {
    WorldState& world;
    const GameRuleValues& gameRules;
    random::RandomSource& random;
    bool ultrawarm = false;
};

[[nodiscard]] std::optional<FluidKind> kindOf(std::int32_t stateId);

/// parity: `FluidState.getHeight`: a full block under the same fluid, else its amount over nine.
[[nodiscard]] float heightAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// Like `kindOf`, but only for a square that **is** the liquid: a waterlogged stair holds water and
/// answers `kindOf`, and it is not a `LiquidBlock`, so the hooks that hang off that class skip it.
[[nodiscard]] std::optional<FluidKind> liquidBlockKindOf(std::int32_t stateId);

/// Runs the fluid at this position, returning every block it changed.
///
/// parity: `FlowingFluid.tick`: a flowing block first settles to whatever its neighbours now
/// justify (which is how a cut-off flow drains), and then spreads from there. A source only spreads.
[[nodiscard]] std::vector<FluidChange> tick(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z);

/// Wakes the fluid **on this square only**, which is what `LiquidBlock.onPlace` is: the block that
/// just landed books its own turn (and lava turns to rock against water) before anybody else hears
/// about the change. Separate from `onBlockChanged` because the order matters: the jar books inside
/// `LevelChunk.setBlockState` and tells the neighbours afterwards, so a sponge that drinks the water
/// away still leaves the turn behind. Measured in `esponja`.
void onArrived(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z);

/// Wakes the fluids around a block that just changed, returning whatever changed on the spot.
///
/// parity: `LiquidBlock.onPlace`/`neighborChanged`/`updateShape`, which all end in the same
/// `scheduleTick`. Lava is the one that does not wait for its tick: water arriving next to it turns
/// it to rock there and then, which is why water reaches a lava flow and never mixes.
[[nodiscard]] std::vector<FluidChange> onBlockChanged(const Context& context, std::int32_t x, std::int32_t y,
                                                      std::int32_t z);

} // namespace fluidFlow

} // namespace basalt::server::runtime::world
