#pragma once

#include <basalt/server/world/structure/MonumentRoomGraph.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::world::structure {

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

struct MonumentRoomPiece {
    MonumentRoomKind kind = MonumentRoomKind::Simple;
    MonumentRoom* room = nullptr;
    std::int32_t design = 0;
};

/// parity: `MonumentBuilding`'s constructor.
[[nodiscard]] std::vector<MonumentRoomPiece> fitMonumentRooms(MonumentRoomGraph& graph, random::RandomSource& random);

} // namespace basalt::server::world::structure
