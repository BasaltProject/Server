#pragma once

#include <basalt/server/world/structure/MonumentRoomGraph.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::world::structure {

/// Which of the monument's room shapes a lattice cell turned into.
enum class MonumentRoomKind : std::uint8_t {
    Entry,
    Core,
    DoubleX,
    DoubleXY,
    DoubleY,
    DoubleYZ,
    DoubleZ,
    Simple,
    SimpleTop,
    LeftWing,
    RightWing,
    Penthouse,
};

/// One room of a monument, chosen but not yet built. `design` is the variant a room draws for itself:
/// the simple room takes one of three, a wing takes the low bit of a single shared draw.
struct MonumentRoomPiece {
    MonumentRoomKind kind = MonumentRoomKind::Simple;
    MonumentRoom* room = nullptr;
    std::int32_t design = 0;
};

/// parity: `MonumentBuilding`'s constructor. Offers every unclaimed cell to the seven fitters in the
/// jar's order and takes the first that fits: that order decides whether a pair of cells becomes one
/// double room or two singles, so it is not alphabetical and must not be tidied.
[[nodiscard]] std::vector<MonumentRoomPiece> fitMonumentRooms(MonumentRoomGraph& graph, random::RandomSource& random);

} // namespace basalt::server::world::structure
