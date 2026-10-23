#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

enum class FluidKind : std::uint8_t { Water, Lava };

/// parity: `FlowingFluid.isSource`.
[[nodiscard]] bool isSourceFluid(std::int32_t stateId);

/// parity: `LiquidBlock` books with `getFluidState().getType()`.
[[nodiscard]] const wire::common::Identifier& fluidId(FluidKind kind, bool source);

[[nodiscard]] std::int32_t baseStateId(FluidKind kind);

struct FluidChange {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t stateId = 0;
    std::int32_t washedAway = 0;
};

/// parity: `FlowingFluid`; a fluid moves only when `onBlockChanged` wakes it.
namespace fluidFlow {

/// parity: `WaterFluid.getTickDelay`, and `LavaFluid`'s, which is faster in the Nether.
inline constexpr std::int32_t kWaterTickDelay = 5;
inline constexpr std::int32_t kLavaTickDelay = 30;
inline constexpr std::int32_t kFastLavaTickDelay = 10;

struct Context {
    WorldState& world;
    const GameRuleValues& gameRules;
    random::RandomSource& random;
    bool ultrawarm = false;
    /// Since 26.3 block tags decide what a flow washes away; without them the older shape rule applies.
    const realBlockTags::TagMembers* blockTags = nullptr;
    /// Asked before the fluid at (x, y, z) spreads into (toX, toY, toZ).
    std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t toX, std::int32_t toY,
                       std::int32_t toZ)>
        flowAllowed;
};

[[nodiscard]] std::optional<FluidKind> kindOf(std::int32_t stateId);

/// parity: `FluidState.getHeight`.
[[nodiscard]] float heightAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] float heightOf(std::int32_t stateId, std::int32_t aboveStateId);

/// parity: `FluidState.getOwnHeight`.
[[nodiscard]] float ownHeightOf(std::int32_t stateId);

/// parity: `FlowingFluid.FALLING`, a flow fed from above.
[[nodiscard]] bool fallingFluid(std::int32_t stateId);

/// A waterlogged stair holds water but is not a `LiquidBlock`, so hooks hanging off that class skip it.
[[nodiscard]] std::optional<FluidKind> liquidBlockKindOf(std::int32_t stateId);

/// parity: `FlowingFluid.tick`.
[[nodiscard]] std::vector<FluidChange> tick(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `LiquidBlock.onPlace`.
void onArrived(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `LiquidBlock.onPlace`/`neighborChanged`/`updateShape`.
[[nodiscard]] std::vector<FluidChange> onBlockChanged(const Context& context, std::int32_t x, std::int32_t y,
                                                      std::int32_t z);

} // namespace fluidFlow

} // namespace basalt::server::runtime::world
