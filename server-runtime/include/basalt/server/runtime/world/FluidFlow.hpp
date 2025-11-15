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

[[nodiscard]] const wire::common::Identifier& fluidId(FluidKind kind);

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

/// Runs the fluid at this position, returning every block it changed.
///
/// parity: `FlowingFluid.tick`: a flowing block first settles to whatever its neighbours now
/// justify (which is how a cut-off flow drains), and then spreads from there. A source only spreads.
[[nodiscard]] std::vector<FluidChange> tick(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z);

/// Wakes the fluids around a block that just changed, returning whatever changed on the spot.
///
/// parity: `LiquidBlock.onPlace`/`neighborChanged`/`updateShape`, which all end in the same
/// `scheduleTick`. Lava is the one that does not wait for its tick: water arriving next to it turns
/// it to rock there and then, which is why water reaches a lava flow and never mixes.
[[nodiscard]] std::vector<FluidChange> onBlockChanged(const Context& context, std::int32_t x, std::int32_t y,
                                                      std::int32_t z);

} // namespace fluidFlow

} // namespace basalt::server::runtime::world
