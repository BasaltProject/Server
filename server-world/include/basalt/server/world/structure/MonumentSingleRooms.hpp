#pragma once

#include <basalt/server/world/structure/MonumentBrush.hpp>
#include <basalt/server/world/structure/MonumentRoomGraph.hpp>

#include <cstdint>

namespace basalt::server::world::structure {

/// The rooms that occupy a single lattice cell, plus the core the whole monument is built around.
/// parity: `OceanMonument{Entry,Simple,SimpleTop,Core}Room`.
namespace monumentSingleRooms {

/// The room the entrance corridor arrives in: three walls and whichever doorways survived.
void entry(const MonumentBrush& brush, const MonumentRoom& room);

/// parity: `OceanMonumentSimpleRoom`. `design` was drawn when the room was chosen; the extra draw
/// for the middle pillar happens here, on the chunk's own random, and only for designs 1 and 2, so a
/// room's interior is not decided by its design alone.
void simple(const MonumentBrush& brush, const MonumentRoom& room, std::int32_t design, random::RandomSource& random);

/// parity: `OceanMonumentSimpleTopRoom`, the sealed cell that grows the sponges.
void simpleTop(const MonumentBrush& brush, const MonumentRoom& room, random::RandomSource& random);

/// parity: `OceanMonumentCoreRoom`, the two by two by two block with the gold in the middle.
void core(const MonumentBrush& brush);

} // namespace monumentSingleRooms

} // namespace basalt::server::world::structure
