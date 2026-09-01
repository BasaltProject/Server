#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// How a rail bends towards the rails around it.
///
/// parity: `RailState`. A rail keeps at most two connections and looks one block up and one down
/// for them; only a plain rail turns a corner, the three that carry a signal may just climb.
namespace rails {

/// Told about every rail this bent, so the caller can send the changes out.
using Changed = std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId)>;

[[nodiscard]] bool isRail(std::int32_t stateId);

/// Bends the rail at these coordinates towards its neighbours, and them towards it.
///
/// parity: `RailState.place` with `first = true`, which is what `BaseRailBlock.onPlace` runs.
/// Answers the shape it settled on, or nothing when there is no rail there.
std::optional<std::int32_t> place(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                  std::int32_t z, const Changed& changed = {});

/// What a rail does when something beside it changes, which is almost always nothing.
///
/// parity: `BaseRailBlock.updateState(state, level, pos, block)` is **empty**, and only `RailBlock`
/// overrides it: a plain rail re-decides its shape when the block that changed gives off a signal
/// *and* it has three ways it could go. That is the T-junction a lever switches, and it is the only
/// case. Re-deciding on every update instead was what made a track laid one rail at a time reroute
/// itself behind you.
std::optional<std::int32_t> onNeighbourChange(WorldState& world, std::string_view worldName, std::int32_t x,
                                              std::int32_t y, std::int32_t z, std::int32_t changedStateId);

} // namespace rails

} // namespace basalt::server::runtime::world
