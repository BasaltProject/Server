#pragma once

#include <basalt/server/world/structure/MonumentBrush.hpp>
#include <basalt/server/world/structure/MonumentRoomGraph.hpp>

namespace basalt::server::world::structure {

/// The double rooms, one doorway per side. parity:
namespace monumentDoubleRooms {

void doubleX(const MonumentBrush& brush, const MonumentRoom& west);
void doubleXy(const MonumentBrush& brush, const MonumentRoom& west);
void doubleY(const MonumentBrush& brush, const MonumentRoom& lower);
void doubleYz(const MonumentBrush& brush, const MonumentRoom& south);
void doubleZ(const MonumentBrush& brush, const MonumentRoom& south);

} // namespace monumentDoubleRooms

} // namespace basalt::server::world::structure
