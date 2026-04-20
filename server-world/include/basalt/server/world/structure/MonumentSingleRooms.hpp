#pragma once

#include <basalt/server/world/structure/MonumentBrush.hpp>
#include <basalt/server/world/structure/MonumentRoomGraph.hpp>

#include <cstdint>

namespace basalt::server::world::structure {

/// parity: `OceanMonument{Entry,Simple,SimpleTop,Core}Room`.
namespace monumentSingleRooms {

void entry(const MonumentBrush& brush, const MonumentRoom& room);

/// parity: `OceanMonumentSimpleRoom`.
void simple(const MonumentBrush& brush, const MonumentRoom& room, std::int32_t design, random::RandomSource& random);

/// parity: `OceanMonumentSimpleTopRoom`, the sealed cell that grows the sponges.
void simpleTop(const MonumentBrush& brush, const MonumentRoom& room, random::RandomSource& random);

/// parity: `OceanMonumentCoreRoom`.
void core(const MonumentBrush& brush);

} // namespace monumentSingleRooms

} // namespace basalt::server::world::structure
