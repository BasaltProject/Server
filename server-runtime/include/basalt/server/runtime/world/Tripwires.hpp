#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

/// The string on the floor and the hooks it runs between.
///
/// parity: `TripWireBlock.checkPressed` for the string, `TripWireHookBlock.calculateState` for the
/// pair of hooks. The hook is the part that gives off redstone, and it only does so when the line
/// between the two is unbroken: a string with nothing at both ends powers nothing.
namespace tripwires {

/// parity: `TripWireBlock.RECHECK_PERIOD`, how long a pressed wire waits before looking again.
inline constexpr std::int32_t kRecheckTicks = 10;

/// parity: `calculateState`'s own 42, the longest a line of string can be.
inline constexpr std::int32_t kMaxLength = 42;

[[nodiscard]] const wire::common::Identifier& wireBlock();
[[nodiscard]] const wire::common::Identifier& hookBlock();

[[nodiscard]] bool isWire(std::int32_t stateId);

[[nodiscard]] bool isHook(std::int32_t stateId);

/// parity: `TripWireBlock.updateShape`, which is what draws a line of string as a line: each
/// horizontal side is set from `shouldConnectTo`, which is another string or a hook facing back.
/// Answers the new state, or nothing when this is not a string or nothing moved.
[[nodiscard]] std::optional<std::int32_t> reshape(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t stateId, std::optional<registry::BlockFace> towards);

/// parity: `TripWireHookBlock.updateShape`: a hook only falls when the news arrives **from the side
/// it hangs on**, and `canSurvive` wants a sturdy face there. From any other side it is left alone,
/// which is why one placed in mid air by command stays until something touches its wall.
[[nodiscard]] bool hookFalls(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                             std::int32_t stateId, registry::BlockFace towards);

/// What is standing on a wire, as its own box: the whole block, since a wire is walked into.
[[nodiscard]] bool isStandingOn(std::int32_t x, std::int32_t y, std::int32_t z, double entityX, double entityY,
                                double entityZ);

/// parity: `checkPressed`: the wire follows whether anything is on it, books its own recheck, and
/// tells the hooks at both ends.
void press(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool occupied, std::int64_t gameTime,
           std::vector<BlockChange>& changes);

/// parity: `TripWireHookBlock.calculateState`: it looks along the way it faces for the hook pointing
/// back at it. Attached means it found one with unbroken string in between; powered means one of
/// those strings has something on it.
/// parity: `TripWireHookBlock`'s own ten ticks: a hook told by a wire that changed looks again that
/// much later, which is what keeps it attached for a moment after the string is cut.
inline constexpr std::int32_t kHookRecheckTicks = 10;

/// `gameTime` is the world's clock, which a hook's game events and vibrations are stamped with.
/// `wireSource` is the square whose state to read from `wireSourceState` instead of the world, `-1`
/// for none: that is how vanilla tells a hook about a wire that has already gone, handing the
/// removed string over as still there and powered so the line fires.
void calculateHook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t gameTime,
                   std::vector<BlockChange>& changes, std::int32_t wireSource = -1,
                   std::optional<std::int32_t> wireSourceState = std::nullopt);

/// parity: `TripWireBlock.updateSource`, which a wire runs when it is placed, when it goes and when
/// it is stepped on. It walks **south and west only**, because `calculateHook` writes both ends of
/// the line, so the hook at the lower end is the only one that has to be found.
void updateSource(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t wireStateId,
                  std::int64_t gameTime, std::vector<BlockChange>& changes);

} // namespace tripwires

} // namespace basalt::server::runtime::world
