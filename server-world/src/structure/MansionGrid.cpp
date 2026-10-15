#include <basalt/server/world/structure/MansionGrid.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::world::structure {

std::int32_t SimpleGrid::at(std::int32_t x, std::int32_t y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        return valueIfOutside_;
    }
    return cells_[static_cast<std::size_t>(x + y * width_)];
}

void SimpleGrid::set(std::int32_t x, std::int32_t y, std::int32_t value) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        return;
    }
    cells_[static_cast<std::size_t>(x + y * width_)] = value;
}

void SimpleGrid::set(std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1, std::int32_t value) {
    for (std::int32_t y = y0; y <= y1; ++y) {
        for (std::int32_t x = x0; x <= x1; ++x) {
            set(x, y, value);
        }
    }
}

void SimpleGrid::setIf(std::int32_t x, std::int32_t y, std::int32_t ifValue, std::int32_t value) {
    if (at(x, y) == ifValue) {
        set(x, y, value);
    }
}

bool SimpleGrid::edgesTo(std::int32_t x, std::int32_t y, std::int32_t ifValue) const {
    return at(x - 1, y) == ifValue || at(x + 1, y) == ifValue || at(x, y + 1) == ifValue || at(x, y - 1) == ifValue;
}

const std::array<StructureDirection, 4>& MansionGrid::horizontal() {
    static const std::array<StructureDirection, 4> order{StructureDirection::North, StructureDirection::East,
                                                         StructureDirection::South, StructureDirection::West};
    return order;
}

const std::array<StructureDirection, 4>& MansionGrid::by2d() {
    static const std::array<StructureDirection, 4> order{StructureDirection::South, StructureDirection::West,
                                                         StructureDirection::North, StructureDirection::East};
    return order;
}

bool MansionGrid::isHouse(const SimpleGrid& grid, std::int32_t x, std::int32_t y) {
    const std::int32_t value = grid.at(x, y);
    return value == kCorridor || value == kRoom || value == kStartRoom || value == 4;
}

StructureDirection MansionGrid::clockWise(StructureDirection direction) {
    switch (direction) {
    case StructureDirection::North:
        return StructureDirection::East;
    case StructureDirection::East:
        return StructureDirection::South;
    case StructureDirection::South:
        return StructureDirection::West;
    default:
        return StructureDirection::North;
    }
}

StructureDirection MansionGrid::counterClockWise(StructureDirection direction) {
    switch (direction) {
    case StructureDirection::North:
        return StructureDirection::West;
    case StructureDirection::West:
        return StructureDirection::South;
    case StructureDirection::South:
        return StructureDirection::East;
    default:
        return StructureDirection::North;
    }
}

MansionGrid::MansionGrid(random::RandomSource& random) : random_(&random) {
    baseGrid_.set(kEntranceX, kEntranceY, kEntranceX + 1, kEntranceY + 1, kStartRoom);
    baseGrid_.set(kEntranceX - 1, kEntranceY, kEntranceX - 1, kEntranceY + 1, kRoom);
    baseGrid_.set(kEntranceX + 2, kEntranceY - 2, kEntranceX + 3, kEntranceY + 3, kBlocked);
    baseGrid_.set(kEntranceX + 1, kEntranceY - 2, kEntranceX + 1, kEntranceY - 1, kCorridor);
    baseGrid_.set(kEntranceX + 1, kEntranceY + 2, kEntranceX + 1, kEntranceY + 3, kCorridor);
    baseGrid_.set(kEntranceX - 1, kEntranceY - 1, kCorridor);
    baseGrid_.set(kEntranceX - 1, kEntranceY + 2, kCorridor);
    baseGrid_.set(0, 0, kSize, 1, kBlocked);
    baseGrid_.set(0, 9, kSize, kSize, kBlocked);
    recursiveCorridor(baseGrid_, kEntranceX, kEntranceY - 2, StructureDirection::West, 6);
    recursiveCorridor(baseGrid_, kEntranceX, kEntranceY + 3, StructureDirection::West, 6);
    recursiveCorridor(baseGrid_, kEntranceX - 2, kEntranceY - 1, StructureDirection::West, 3);
    recursiveCorridor(baseGrid_, kEntranceX - 2, kEntranceY + 2, StructureDirection::West, 3);

    while (cleanEdges(baseGrid_)) {
    }

    identifyRooms(baseGrid_, floorRooms_[0]);
    identifyRooms(baseGrid_, floorRooms_[1]);
    floorRooms_[0].set(kEntranceX + 1, kEntranceY, kEntranceX + 1, kEntranceY + 1, kRoomCorridorFlag);
    floorRooms_[1].set(kEntranceX + 1, kEntranceY, kEntranceX + 1, kEntranceY + 1, kRoomCorridorFlag);
    setupThirdFloor();
    identifyRooms(thirdFloorGrid_, floorRooms_[2]);
}

bool MansionGrid::isRoomId(std::int32_t x, std::int32_t y, std::size_t floor, std::int32_t roomId) const {
    return (floorRooms_[floor].at(x, y) & kRoomIdMask) == roomId;
}

std::optional<StructureDirection> MansionGrid::roomDirection1x2(std::int32_t x, std::int32_t y, std::size_t floor,
                                                                std::int32_t roomId) const {
    for (const StructureDirection direction : horizontal()) {
        if (isRoomId(x + dx(direction), y + dz(direction), floor, roomId)) {
            return direction;
        }
    }
    return std::nullopt;
}

void MansionGrid::recursiveCorridor(SimpleGrid& grid, std::int32_t x, std::int32_t y, StructureDirection heading,
                                    std::int32_t depth) {
    if (depth <= 0) {
        return;
    }
    grid.set(x, y, kCorridor);
    grid.setIf(x + dx(heading), y + dz(heading), kClear, kCorridor);

    for (std::int32_t attempt = 0; attempt < 8; ++attempt) {
        const StructureDirection next = by2d()[static_cast<std::size_t>(random_->nextInt(4))];
        if (next != opposite(heading) && (next != StructureDirection::East || !random_->nextBoolean())) {
            const std::int32_t nx = x + dx(heading);
            const std::int32_t ny = y + dz(heading);
            if (grid.at(nx + dx(next), ny + dz(next)) == kClear &&
                grid.at(nx + dx(next) * 2, ny + dz(next) * 2) == kClear) {
                recursiveCorridor(grid, x + dx(heading) + dx(next), y + dz(heading) + dz(next), next, depth - 1);
                break;
            }
        }
    }

    const StructureDirection cw = clockWise(heading);
    const StructureDirection ccw = counterClockWise(heading);
    grid.setIf(x + dx(cw), y + dz(cw), kClear, kRoom);
    grid.setIf(x + dx(ccw), y + dz(ccw), kClear, kRoom);
    grid.setIf(x + dx(heading) + dx(cw), y + dz(heading) + dz(cw), kClear, kRoom);
    grid.setIf(x + dx(heading) + dx(ccw), y + dz(heading) + dz(ccw), kClear, kRoom);
    grid.setIf(x + dx(heading) * 2, y + dz(heading) * 2, kClear, kRoom);
    grid.setIf(x + dx(cw) * 2, y + dz(cw) * 2, kClear, kRoom);
    grid.setIf(x + dx(ccw) * 2, y + dz(ccw) * 2, kClear, kRoom);
}

bool MansionGrid::cleanEdges(SimpleGrid& grid) {
    bool touched = false;
    for (std::int32_t y = 0; y < grid.height(); ++y) {
        for (std::int32_t x = 0; x < grid.width(); ++x) {
            if (grid.at(x, y) != kClear) {
                continue;
            }
            std::int32_t direct = 0;
            direct += isHouse(grid, x + 1, y) ? 1 : 0;
            direct += isHouse(grid, x - 1, y) ? 1 : 0;
            direct += isHouse(grid, x, y + 1) ? 1 : 0;
            direct += isHouse(grid, x, y - 1) ? 1 : 0;
            if (direct >= 3) {
                grid.set(x, y, kRoom);
                touched = true;
            } else if (direct == 2) {
                std::int32_t diagonal = 0;
                diagonal += isHouse(grid, x + 1, y + 1) ? 1 : 0;
                diagonal += isHouse(grid, x - 1, y + 1) ? 1 : 0;
                diagonal += isHouse(grid, x + 1, y - 1) ? 1 : 0;
                diagonal += isHouse(grid, x - 1, y - 1) ? 1 : 0;
                if (diagonal <= 1) {
                    grid.set(x, y, kRoom);
                    touched = true;
                }
            }
        }
    }
    return touched;
}

void MansionGrid::setupThirdFloor() {
    std::vector<std::pair<std::int32_t, std::int32_t>> potential;
    SimpleGrid& floor = floorRooms_[1];
    for (std::int32_t y = 0; y < thirdFloorGrid_.height(); ++y) {
        for (std::int32_t x = 0; x < thirdFloorGrid_.width(); ++x) {
            const std::int32_t data = floor.at(x, y);
            if ((data & kRoomTypeMask) == kRoom1x2 && (data & kRoomDoorFlag) == kRoomDoorFlag) {
                potential.emplace_back(x, y);
            }
        }
    }
    if (potential.empty()) {
        thirdFloorGrid_.set(0, 0, thirdFloorGrid_.width(), thirdFloorGrid_.height(), kBlocked);
        return;
    }
    const auto& [roomX, roomY] =
        potential[static_cast<std::size_t>(random_->nextInt(static_cast<std::int32_t>(potential.size())))];
    const std::int32_t data = floor.at(roomX, roomY);
    floor.set(roomX, roomY, data | kRoomStairsFlag);
    const std::optional<StructureDirection> direction = roomDirection1x2(roomX, roomY, 1, data & kRoomIdMask);
    if (!direction.has_value()) {
        return;
    }
    const std::int32_t endX = roomX + dx(*direction);
    const std::int32_t endY = roomY + dz(*direction);

    for (std::int32_t y = 0; y < thirdFloorGrid_.height(); ++y) {
        for (std::int32_t x = 0; x < thirdFloorGrid_.width(); ++x) {
            if (!isHouse(baseGrid_, x, y)) {
                thirdFloorGrid_.set(x, y, kBlocked);
            } else if (x == roomX && y == roomY) {
                thirdFloorGrid_.set(x, y, kStartRoom);
            } else if (x == endX && y == endY) {
                thirdFloorGrid_.set(x, y, kStartRoom);
                floorRooms_[2].set(x, y, kRoomCorridorFlag);
            }
        }
    }

    std::vector<StructureDirection> corridors;
    for (const StructureDirection candidate : horizontal()) {
        if (thirdFloorGrid_.at(endX + dx(candidate), endY + dz(candidate)) == kClear) {
            corridors.push_back(candidate);
        }
    }
    if (corridors.empty()) {
        thirdFloorGrid_.set(0, 0, thirdFloorGrid_.width(), thirdFloorGrid_.height(), kBlocked);
        floor.set(roomX, roomY, data);
        return;
    }
    const StructureDirection corridor =
        corridors[static_cast<std::size_t>(random_->nextInt(static_cast<std::int32_t>(corridors.size())))];
    recursiveCorridor(thirdFloorGrid_, endX + dx(corridor), endY + dz(corridor), corridor, 4);

    while (cleanEdges(thirdFloorGrid_)) {
    }
}

void MansionGrid::shuffle(std::vector<std::pair<std::int32_t, std::int32_t>>& list) {
    for (auto size = static_cast<std::int32_t>(list.size()); size >= 2; --size) {
        const std::int32_t swapTo = random_->nextInt(size);
        std::swap(list[static_cast<std::size_t>(size - 1)], list[static_cast<std::size_t>(swapTo)]);
    }
}

void MansionGrid::identifyRooms(const SimpleGrid& fromGrid, SimpleGrid& roomGrid) {
    std::vector<std::pair<std::int32_t, std::int32_t>> positions;
    for (std::int32_t y = 0; y < fromGrid.height(); ++y) {
        for (std::int32_t x = 0; x < fromGrid.width(); ++x) {
            if (fromGrid.at(x, y) == kRoom) {
                positions.emplace_back(x, y);
            }
        }
    }
    shuffle(positions);
    std::int32_t roomId = 10;

    for (const auto& [x, y] : positions) {
        if (roomGrid.at(x, y) != kClear) {
            continue;
        }
        std::int32_t x0 = x;
        std::int32_t x1 = x;
        std::int32_t y0 = y;
        std::int32_t y1 = y;
        std::int32_t type = kRoom1x1;
        if (roomGrid.at(x + 1, y) == kClear && roomGrid.at(x, y + 1) == kClear && roomGrid.at(x + 1, y + 1) == kClear &&
            fromGrid.at(x + 1, y) == kRoom && fromGrid.at(x, y + 1) == kRoom && fromGrid.at(x + 1, y + 1) == kRoom) {
            ++x1;
            ++y1;
            type = kRoom2x2;
        } else if (roomGrid.at(x - 1, y) == kClear && roomGrid.at(x, y + 1) == kClear &&
                   roomGrid.at(x - 1, y + 1) == kClear && fromGrid.at(x - 1, y) == kRoom &&
                   fromGrid.at(x, y + 1) == kRoom && fromGrid.at(x - 1, y + 1) == kRoom) {
            --x0;
            ++y1;
            type = kRoom2x2;
        } else if (roomGrid.at(x - 1, y) == kClear && roomGrid.at(x, y - 1) == kClear &&
                   roomGrid.at(x - 1, y - 1) == kClear && fromGrid.at(x - 1, y) == kRoom &&
                   fromGrid.at(x, y - 1) == kRoom && fromGrid.at(x - 1, y - 1) == kRoom) {
            --x0;
            --y0;
            type = kRoom2x2;
        } else if (roomGrid.at(x + 1, y) == kClear && fromGrid.at(x + 1, y) == kRoom) {
            ++x1;
            type = kRoom1x2;
        } else if (roomGrid.at(x, y + 1) == kClear && fromGrid.at(x, y + 1) == kRoom) {
            ++y1;
            type = kRoom1x2;
        } else if (roomGrid.at(x - 1, y) == kClear && fromGrid.at(x - 1, y) == kRoom) {
            --x0;
            type = kRoom1x2;
        } else if (roomGrid.at(x, y - 1) == kClear && fromGrid.at(x, y - 1) == kRoom) {
            --y0;
            type = kRoom1x2;
        }

        std::int32_t doorX = random_->nextBoolean() ? x0 : x1;
        std::int32_t doorY = random_->nextBoolean() ? y0 : y1;
        std::int32_t doorFlag = kRoomDoorFlag;
        // Four corners tried in a fixed dance, and the door is dropped only when none of them touches a corridor.
        if (!fromGrid.edgesTo(doorX, doorY, kCorridor)) {
            doorX = doorX == x0 ? x1 : x0;
            doorY = doorY == y0 ? y1 : y0;
            if (!fromGrid.edgesTo(doorX, doorY, kCorridor)) {
                doorY = doorY == y0 ? y1 : y0;
                if (!fromGrid.edgesTo(doorX, doorY, kCorridor)) {
                    doorX = doorX == x0 ? x1 : x0;
                    doorY = doorY == y0 ? y1 : y0;
                    if (!fromGrid.edgesTo(doorX, doorY, kCorridor)) {
                        doorFlag = 0;
                        doorX = x0;
                        doorY = y0;
                    }
                }
            }
        }

        for (std::int32_t ry = y0; ry <= y1; ++ry) {
            for (std::int32_t rx = x0; rx <= x1; ++rx) {
                if (rx == doorX && ry == doorY) {
                    roomGrid.set(rx, ry, kRoomOriginFlag | doorFlag | type | roomId);
                } else {
                    roomGrid.set(rx, ry, type | roomId);
                }
            }
        }
        ++roomId;
    }
}

} // namespace basalt::server::world::structure
