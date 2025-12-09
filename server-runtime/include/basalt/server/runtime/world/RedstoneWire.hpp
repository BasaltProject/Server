#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::runtime::world {

/// Which way a line of redstone dust runs.
///
/// parity: `RedStoneWireBlock.getConnectionState`. A dust reaches towards whatever it can carry a
/// signal to, climbs one block when the way up is clear, and when it finds nothing on one axis it
/// straightens out along the other rather than sitting there as a dot.
namespace redstoneWire {

[[nodiscard]] const wire::common::Identifier& block();

[[nodiscard]] bool isWire(std::int32_t stateId);

/// parity: `getConnectionState`, including the "a dot stays a dot" clause.
[[nodiscard]] std::map<std::string, std::string> connectionsFor(WorldState& world, std::int32_t x, std::int32_t y,
                                                                std::int32_t z, std::int32_t stateId);

/// The shape the dust at these coordinates should have, written if it moved. Answers the new state,
/// or nothing when it was already right.
[[nodiscard]] std::optional<std::int32_t> reshape(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t stateId);

} // namespace redstoneWire

} // namespace basalt::server::runtime::world
