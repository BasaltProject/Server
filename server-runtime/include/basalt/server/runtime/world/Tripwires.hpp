#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
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
/// `gameTime` is the world's clock: a hook that catches, lets go, clicks on or clicks off raises one
/// of four game events, and a vibration is stamped with the turn it was heard on.
void calculateHook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t gameTime,
                   std::vector<BlockChange>& changes);

} // namespace tripwires

} // namespace basalt::server::runtime::world
