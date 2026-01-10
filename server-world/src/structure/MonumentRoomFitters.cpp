#include <basalt/server/world/structure/MonumentRoomFitters.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::world::structure {
namespace {

[[nodiscard]] std::size_t ordinal(StructureDirection direction) { return static_cast<std::size_t>(direction); }

[[nodiscard]] MonumentRoom* neighbour(const MonumentRoom& room, StructureDirection direction) {
    return room.connection(ordinal(direction));
}

[[nodiscard]] bool open(const MonumentRoom& room, StructureDirection direction) {
    const MonumentRoom* next = neighbour(room, direction);
    return room.hasOpening(ordinal(direction)) && next != nullptr && !next->claimed();
}

void claimIfPresent(MonumentRoom* room) {
    if (room != nullptr) {
        room->claim();
    }
}

using Fitter = std::function<std::optional<MonumentRoomPiece>(MonumentRoom&, random::RandomSource&)>;

/// parity: the seven fitters, in the order `MonumentBuilding` registers them.
const std::vector<Fitter>& fitters() {
    static const std::vector<Fitter> built = {
        // Double on X and Y: the cell, its east neighbour and both of their ceilings.
        [](MonumentRoom& room, random::RandomSource&) -> std::optional<MonumentRoomPiece> {
            MonumentRoom* east = neighbour(room, StructureDirection::East);
            if (!open(room, StructureDirection::East) || !open(room, StructureDirection::Up) || east == nullptr ||
                !open(*east, StructureDirection::Up)) {
                return std::nullopt;
            }
            room.claim();
            east->claim();
            claimIfPresent(neighbour(room, StructureDirection::Up));
            claimIfPresent(neighbour(*east, StructureDirection::Up));
            return MonumentRoomPiece{.kind = MonumentRoomKind::DoubleXY, .room = &room};
        },
        // Double on Y and Z, the same shape turned onto the north face.
        [](MonumentRoom& room, random::RandomSource&) -> std::optional<MonumentRoomPiece> {
            MonumentRoom* north = neighbour(room, StructureDirection::North);
            if (!open(room, StructureDirection::North) || !open(room, StructureDirection::Up) || north == nullptr ||
                !open(*north, StructureDirection::Up)) {
                return std::nullopt;
            }
            room.claim();
            north->claim();
            claimIfPresent(neighbour(room, StructureDirection::Up));
            claimIfPresent(neighbour(*north, StructureDirection::Up));
            return MonumentRoomPiece{.kind = MonumentRoomKind::DoubleYZ, .room = &room};
        },
        // Double on Z. The jar falls back to the south neighbour here, but its own `fits` already
        // guarantees the north door is open and free, so that branch is unreachable; it is kept
        // because reading the jar and finding it missing would look like an omission.
        [](MonumentRoom& room, random::RandomSource&) -> std::optional<MonumentRoomPiece> {
            if (!open(room, StructureDirection::North)) {
                return std::nullopt;
            }
            MonumentRoom* northNeighbour = neighbour(room, StructureDirection::North);
            MonumentRoom* anchor = room.hasOpening(ordinal(StructureDirection::North)) && northNeighbour != nullptr &&
                                           !northNeighbour->claimed()
                                       ? &room
                                       : neighbour(room, StructureDirection::South);
            if (anchor == nullptr) {
                return std::nullopt;
            }
            anchor->claim();
            claimIfPresent(neighbour(*anchor, StructureDirection::North));
            return MonumentRoomPiece{.kind = MonumentRoomKind::DoubleZ, .room = anchor};
        },
        [](MonumentRoom& room, random::RandomSource&) -> std::optional<MonumentRoomPiece> {
            if (!open(room, StructureDirection::East)) {
                return std::nullopt;
            }
            room.claim();
            claimIfPresent(neighbour(room, StructureDirection::East));
            return MonumentRoomPiece{.kind = MonumentRoomKind::DoubleX, .room = &room};
        },
        [](MonumentRoom& room, random::RandomSource&) -> std::optional<MonumentRoomPiece> {
            if (!open(room, StructureDirection::Up)) {
                return std::nullopt;
            }
            room.claim();
            claimIfPresent(neighbour(room, StructureDirection::Up));
            return MonumentRoomPiece{.kind = MonumentRoomKind::DoubleY, .room = &room};
        },
        // A cell with no doorway at all but the floor becomes the little top room.
        [](MonumentRoom& room, random::RandomSource&) -> std::optional<MonumentRoomPiece> {
            const bool sealed = !room.hasOpening(ordinal(StructureDirection::West)) &&
                                !room.hasOpening(ordinal(StructureDirection::East)) &&
                                !room.hasOpening(ordinal(StructureDirection::North)) &&
                                !room.hasOpening(ordinal(StructureDirection::South)) &&
                                !room.hasOpening(ordinal(StructureDirection::Up));
            if (!sealed) {
                return std::nullopt;
            }
            room.claim();
            return MonumentRoomPiece{.kind = MonumentRoomKind::SimpleTop, .room = &room};
        },
        // The catch-all, and the only fitter that draws: one of three interiors.
        [](MonumentRoom& room, random::RandomSource& random) -> std::optional<MonumentRoomPiece> {
            room.claim();
            return MonumentRoomPiece{.kind = MonumentRoomKind::Simple, .room = &room, .design = random.nextInt(3)};
        }};
    return built;
}

} // namespace

std::vector<MonumentRoomPiece> fitMonumentRooms(MonumentRoomGraph& graph, random::RandomSource& random) {
    std::vector<MonumentRoomPiece> pieces;
    graph.sourceRoom->claim();
    pieces.push_back(MonumentRoomPiece{.kind = MonumentRoomKind::Entry, .room = graph.sourceRoom});
    pieces.push_back(MonumentRoomPiece{.kind = MonumentRoomKind::Core, .room = graph.coreRoom});

    for (MonumentRoom* room : graph.rooms) {
        if (room->claimed() || room->isSpecial()) {
            continue;
        }
        for (const Fitter& fitter : fitters()) {
            const std::optional<MonumentRoomPiece> fitted = fitter(*room, random);
            if (fitted.has_value()) {
                pieces.push_back(*fitted);
                break;
            }
        }
    }

    // One draw for both wings, and the second takes the value plus one, so their designs differ.
    const std::int32_t wingDraw = random.nextInt();
    pieces.push_back(MonumentRoomPiece{.kind = MonumentRoomKind::LeftWing, .design = wingDraw & 1});
    pieces.push_back(MonumentRoomPiece{.kind = MonumentRoomKind::RightWing, .design = (wingDraw + 1) & 1});
    pieces.push_back(MonumentRoomPiece{.kind = MonumentRoomKind::Penthouse});
    return pieces;
}

} // namespace basalt::server::world::structure
