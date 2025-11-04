#include <basalt/server/world/structure/MonumentRoomGraph.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::world::structure {
namespace {

/// parity: `Direction.values` / `get3DDataValue`, which is down, up, north, south, west, east.
constexpr std::array<StructureDirection, 6> kOrder{StructureDirection::Down,  StructureDirection::Up,
                                                   StructureDirection::North, StructureDirection::South,
                                                   StructureDirection::West,  StructureDirection::East};

[[nodiscard]] std::size_t ordinal(StructureDirection direction) { return static_cast<std::size_t>(direction); }

/// parity: `Util.shuffle`.
void shuffle(std::vector<MonumentRoom*>& list, random::RandomSource& random) {
    for (auto size = static_cast<std::int32_t>(list.size()); size >= 2; --size) {
        const std::int32_t swapTo = random.nextInt(size);
        std::swap(list[static_cast<std::size_t>(size - 1)], list[static_cast<std::size_t>(swapTo)]);
    }
}

} // namespace

void MonumentRoom::connect(StructureDirection direction, MonumentRoom& other) {
    connections_[ordinal(direction)] = &other;
    other.connections_[ordinal(opposite(direction))] = this;
}

void MonumentRoom::updateOpenings() {
    for (std::size_t face = 0; face < 6; ++face) {
        hasOpening_[face] = connections_[face] != nullptr;
    }
}

bool MonumentRoom::findSource(std::int32_t scan) {
    if (isSource_) {
        return true;
    }
    scanIndex_ = scan;
    for (std::size_t face = 0; face < 6; ++face) {
        MonumentRoom* next = connections_[face];
        if (next == nullptr) {
            continue;
        }
        if (hasOpening_[face] && next->scanIndex_ != scan && next->findSource(scan)) {
            return true;
        }
    }
    return false;
}

std::int32_t MonumentRoom::countOpenings() const {
    return static_cast<std::int32_t>(std::count(hasOpening_.begin(), hasOpening_.end(), true));
}

MonumentRoomGraph generateMonumentRoomGraph(random::RandomSource& random) {
    MonumentRoomGraph graph;
    std::array<MonumentRoom*, 75> grid{};
    const auto add = [&graph, &grid](std::int32_t index) {
        graph.owned.push_back(std::make_unique<MonumentRoom>(index));
        grid[static_cast<std::size_t>(index)] = graph.owned.back().get();
    };
    for (std::int32_t x = 0; x < 5; ++x) {
        for (std::int32_t z = 0; z < 4; ++z) {
            add(roomIndex(x, 0, z));
        }
    }
    for (std::int32_t x = 0; x < 5; ++x) {
        for (std::int32_t z = 0; z < 4; ++z) {
            add(roomIndex(x, 1, z));
        }
    }
    for (std::int32_t x = 1; x < 4; ++x) {
        for (std::int32_t z = 0; z < 2; ++z) {
            add(roomIndex(x, 2, z));
        }
    }

    MonumentRoom* sourceRoom = grid[static_cast<std::size_t>(kMonumentSourceIndex)];

    for (std::int32_t x = 0; x < 5; ++x) {
        for (std::int32_t z = 0; z < 5; ++z) {
            for (std::int32_t y = 0; y < 3; ++y) {
                MonumentRoom* room = grid[static_cast<std::size_t>(roomIndex(x, y, z))];
                if (room == nullptr) {
                    continue;
                }
                for (const StructureDirection direction : kOrder) {
                    const std::int32_t nx = x + dx(direction);
                    const std::int32_t ny = y + dy(direction);
                    const std::int32_t nz = z + dz(direction);
                    if (nx < 0 || nx >= 5 || nz < 0 || nz >= 5 || ny < 0 || ny >= 3) {
                        continue;
                    }
                    MonumentRoom* neighbour = grid[static_cast<std::size_t>(roomIndex(nx, ny, nz))];
                    if (neighbour == nullptr) {
                        continue;
                    }
                    // parity: a neighbour on the Z axis is linked through the opposite face. It reads
                    // like a slip and it is not one: the lattice is built with it.
                    room->connect(nz == z ? direction : opposite(direction), *neighbour);
                }
            }
        }
    }

    graph.owned.push_back(std::make_unique<MonumentRoom>(kMonumentPenthouseIndex));
    MonumentRoom* roofRoom = graph.owned.back().get();
    graph.owned.push_back(std::make_unique<MonumentRoom>(kMonumentLeftWingIndex));
    MonumentRoom* leftWing = graph.owned.back().get();
    graph.owned.push_back(std::make_unique<MonumentRoom>(kMonumentRightWingIndex));
    MonumentRoom* rightWing = graph.owned.back().get();
    grid[static_cast<std::size_t>(kMonumentTopConnectIndex)]->connect(StructureDirection::Up, *roofRoom);
    grid[static_cast<std::size_t>(kMonumentLeftWingConnectIndex)]->connect(StructureDirection::South, *leftWing);
    grid[static_cast<std::size_t>(kMonumentRightWingConnectIndex)]->connect(StructureDirection::South, *rightWing);
    roofRoom->claim();
    leftWing->claim();
    rightWing->claim();
    sourceRoom->markSource();

    MonumentRoom* coreRoom = grid[static_cast<std::size_t>(roomIndex(random.nextInt(4), 0, 2))];
    coreRoom->claim();
    const std::size_t east = ordinal(StructureDirection::East);
    const std::size_t north = ordinal(StructureDirection::North);
    const std::size_t up = ordinal(StructureDirection::Up);
    const auto claimThrough = [](MonumentRoom* room, std::initializer_list<std::size_t> faces) {
        for (const std::size_t face : faces) {
            if (room == nullptr) {
                return;
            }
            room = room->connection(face);
        }
        if (room != nullptr) {
            room->claim();
        }
    };
    claimThrough(coreRoom, {east});
    claimThrough(coreRoom, {north});
    claimThrough(coreRoom, {east, north});
    claimThrough(coreRoom, {up});
    claimThrough(coreRoom, {east, up});
    claimThrough(coreRoom, {north, up});
    claimThrough(coreRoom, {east, north, up});

    for (MonumentRoom* room : grid) {
        if (room == nullptr) {
            continue;
        }
        room->updateOpenings();
        graph.rooms.push_back(room);
    }
    roofRoom->updateOpenings();
    shuffle(graph.rooms, random);

    std::int32_t scan = 1;
    for (MonumentRoom* room : graph.rooms) {
        std::int32_t closed = 0;
        std::int32_t attempts = 0;
        while (closed < 2 && attempts < 5) {
            ++attempts;
            const auto face = static_cast<std::size_t>(random.nextInt(6));
            if (!room->hasOpening(face)) {
                continue;
            }
            MonumentRoom* neighbour = room->connection(face);
            if (neighbour == nullptr) {
                continue;
            }
            const std::size_t back = ordinal(opposite(kOrder[face]));
            room->setOpening(face, false);
            neighbour->setOpening(back, false);
            // The doorway stays shut only if both rooms can still reach the entrance without it.
            if (room->findSource(scan++) && neighbour->findSource(scan++)) {
                ++closed;
            } else {
                room->setOpening(face, true);
                neighbour->setOpening(back, true);
            }
        }
    }

    graph.rooms.push_back(roofRoom);
    graph.rooms.push_back(leftWing);
    graph.rooms.push_back(rightWing);
    graph.sourceRoom = sourceRoom;
    graph.coreRoom = coreRoom;
    return graph;
}

} // namespace basalt::server::world::structure
