#include <basalt/server/world/structure/MansionPlacer.hpp>

namespace basalt::server::world::structure {
namespace {

[[nodiscard]] StructurePos above(StructurePos pos, std::int32_t distance) {
    return StructurePos{pos.x, pos.y + distance, pos.z};
}

/// The corridor tiles, each with a carpet on any side that carries on being corridor.
void corridorFloor(std::vector<MansionPiece>& pieces, StructurePos floorOrigin, StructureRotation rotation,
                   const SimpleGrid& grid, const SimpleGrid& roomGrid, std::size_t floor, std::int32_t startX,
                   std::int32_t startY) {
    const char* southPiece = floor == 0 ? "carpet_south_1" : "carpet_south_2";
    const char* westPiece = floor == 0 ? "carpet_west_1" : "carpet_west_2";
    const StructureDirection east = rotateDirection(rotation, StructureDirection::East);
    const StructureDirection west = rotateDirection(rotation, StructureDirection::West);
    const StructureDirection north = rotateDirection(rotation, StructureDirection::North);
    const StructureDirection south = rotateDirection(rotation, StructureDirection::South);

    const auto carpeted = [&grid, &roomGrid](std::int32_t x, std::int32_t y) {
        return grid.at(x, y) == MansionGrid::kCorridor ||
               (roomGrid.at(x, y) & MansionGrid::kRoomCorridorFlag) == MansionGrid::kRoomCorridorFlag;
    };

    for (std::int32_t y = 0; y < grid.height(); ++y) {
        for (std::int32_t x = 0; x < grid.width(); ++x) {
            if (grid.at(x, y) != MansionGrid::kCorridor) {
                continue;
            }
            StructurePos pos = relative(floorOrigin, south, 8 + (y - startY) * 8);
            pos = relative(pos, east, (x - startX) * 8);
            pieces.push_back(MansionPiece{.templateName = "corridor_floor", .position = pos, .rotation = rotation});
            if (carpeted(x, y - 1)) {
                pieces.push_back(MansionPiece{.templateName = "carpet_north",
                                              .position = above(relative(pos, east, 1), 1),
                                              .rotation = rotation});
            }
            if (carpeted(x + 1, y)) {
                pieces.push_back(MansionPiece{.templateName = "carpet_east",
                                              .position = above(relative(relative(pos, south, 1), east, 5), 1),
                                              .rotation = rotation});
            }
            if (carpeted(x, y + 1)) {
                pieces.push_back(MansionPiece{.templateName = southPiece,
                                              .position = relative(relative(pos, south, 5), west, 1),
                                              .rotation = rotation});
            }
            if (carpeted(x - 1, y)) {
                pieces.push_back(MansionPiece{.templateName = westPiece,
                                              .position = relative(relative(pos, west, 1), north, 1),
                                              .rotation = rotation});
            }
        }
    }
}

void fillRoom(std::vector<MansionPiece>& pieces, random::RandomSource& random, StructurePos roomPos,
              StructureRotation rotation, const MansionGrid& mansion, std::size_t floor, std::int32_t x, std::int32_t y,
              std::int32_t roomId, std::int32_t roomType, std::int32_t roomData,
              std::optional<StructureDirection> doorDirection) {
    if (roomType == MansionGrid::kRoom1x1) {
        mansionRoomFiller::addRoom1x1(pieces, random, roomPos, rotation, doorDirection, floor);
        return;
    }
    if (roomType == MansionGrid::kRoom1x2 && doorDirection.has_value()) {
        const std::optional<StructureDirection> roomDirection = mansion.roomDirection1x2(x, y, floor, roomId);
        const bool isStairs = (roomData & MansionGrid::kRoomStairsFlag) == MansionGrid::kRoomStairsFlag;
        mansionRoomFiller::addRoom1x2(pieces, random, roomPos, rotation, roomDirection, *doorDirection, floor,
                                      isStairs);
        return;
    }
    if (roomType == MansionGrid::kRoom2x2 && doorDirection.has_value() && *doorDirection != StructureDirection::Up) {
        // The 2x2 is entered from one of its two halves, so which way it opens is the door's
        // clockwise neighbour, or the other side when that one is not part of the same room.
        StructureDirection roomDirection = MansionGrid::clockWise(*doorDirection);
        if (!mansion.isRoomId(x + dx(roomDirection), y + dz(roomDirection), floor, roomId)) {
            roomDirection = opposite(roomDirection);
        }
        mansionRoomFiller::addRoom2x2(pieces, random, roomPos, rotation, roomDirection, *doorDirection, floor);
        return;
    }
    if (roomType == MansionGrid::kRoom2x2 && doorDirection == StructureDirection::Up) {
        mansionRoomFiller::addRoom2x2Secret(pieces, roomPos, rotation, floor);
    }
}

/// The four walls of every room, one of which becomes its door, and then whatever fills it.
void roomWalls(std::vector<MansionPiece>& pieces, random::RandomSource& random, StructurePos floorOrigin,
               StructureRotation rotation, const MansionGrid& mansion, const SimpleGrid& grid,
               const SimpleGrid& roomGrid, std::size_t floor, std::int32_t startX, std::int32_t startY) {
    const char* wallPiece = floor == 0 ? "indoors_wall_1" : "indoors_wall_2";
    const char* doorPiece = floor == 0 ? "indoors_door_1" : "indoors_door_2";
    const StructureDirection east = rotateDirection(rotation, StructureDirection::East);
    const StructureDirection north = rotateDirection(rotation, StructureDirection::North);
    const StructureDirection south = rotateDirection(rotation, StructureDirection::South);
    const StructureRotation quarter = rotated(rotation, StructureRotation::Clockwise90);

    for (std::int32_t y = 0; y < grid.height(); ++y) {
        for (std::int32_t x = 0; x < grid.width(); ++x) {
            bool towerRoom = floor == 2 && grid.at(x, y) == MansionGrid::kStartRoom;
            if (grid.at(x, y) != MansionGrid::kRoom && !towerRoom) {
                continue;
            }
            const std::int32_t roomData = roomGrid.at(x, y);
            const std::int32_t roomType = roomData & MansionGrid::kRoomTypeMask;
            const std::int32_t roomId = roomData & MansionGrid::kRoomIdMask;
            towerRoom = towerRoom && (roomData & MansionGrid::kRoomCorridorFlag) == MansionGrid::kRoomCorridorFlag;

            std::vector<StructureDirection> doorDirections;
            if ((roomData & MansionGrid::kRoomDoorFlag) == MansionGrid::kRoomDoorFlag) {
                for (const StructureDirection candidate : MansionGrid::horizontal()) {
                    if (grid.at(x + dx(candidate), y + dz(candidate)) == MansionGrid::kCorridor) {
                        doorDirections.push_back(candidate);
                    }
                }
            }
            std::optional<StructureDirection> doorDirection;
            if (!doorDirections.empty()) {
                doorDirection = doorDirections[static_cast<std::size_t>(
                    random.nextInt(static_cast<std::int32_t>(doorDirections.size())))];
            } else if ((roomData & MansionGrid::kRoomOriginFlag) == MansionGrid::kRoomOriginFlag) {
                doorDirection = StructureDirection::Up;
            }

            StructurePos roomPos = relative(floorOrigin, south, 8 + (y - startY) * 8);
            roomPos = relative(roomPos, east, -1 + (x - startX) * 8);

            if (MansionGrid::isHouse(grid, x - 1, y) && !mansion.isRoomId(x - 1, y, floor, roomId)) {
                const char* name = doorDirection == StructureDirection::West ? doorPiece : wallPiece;
                pieces.push_back(MansionPiece{.templateName = name, .position = roomPos, .rotation = rotation});
            }
            if (grid.at(x + 1, y) == MansionGrid::kCorridor && !towerRoom) {
                const char* name = doorDirection == StructureDirection::East ? doorPiece : wallPiece;
                pieces.push_back(
                    MansionPiece{.templateName = name, .position = relative(roomPos, east, 8), .rotation = rotation});
            }
            if (MansionGrid::isHouse(grid, x, y + 1) && !mansion.isRoomId(x, y + 1, floor, roomId)) {
                const char* name = doorDirection == StructureDirection::South ? doorPiece : wallPiece;
                pieces.push_back(MansionPiece{.templateName = name,
                                              .position = relative(relative(roomPos, south, 7), east, 7),
                                              .rotation = quarter});
            }
            if (grid.at(x, y - 1) == MansionGrid::kCorridor && !towerRoom) {
                const char* name = doorDirection == StructureDirection::North ? doorPiece : wallPiece;
                pieces.push_back(MansionPiece{.templateName = name,
                                              .position = relative(relative(roomPos, north, 1), east, 7),
                                              .rotation = quarter});
            }

            fillRoom(pieces, random, roomPos, rotation, mansion, floor, x, y, roomId, roomType, roomData,
                     doorDirection);
        }
    }
}

} // namespace

void mansionPlacer::createMansion(std::vector<MansionPiece>& pieces, random::RandomSource& random, StructurePos origin,
                                  StructureRotation rotation, const MansionGrid& mansion) {
    const std::int32_t startX = MansionGrid::kEntranceX + 1;
    const std::int32_t startY = MansionGrid::kEntranceY + 1;
    const MansionExterior exterior{startX, startY};

    MansionPlacement data{.position = origin, .rotation = rotation, .wallType = "wall_flat"};
    exterior.entrance(pieces, data);
    MansionPlacement secondData{
        .position = above(data.position, 8), .rotation = data.rotation, .wallType = "wall_window"};

    const SimpleGrid& baseGrid = mansion.baseGrid();
    const SimpleGrid& thirdGrid = mansion.thirdFloorGrid();
    const std::int32_t endX = MansionGrid::kEntranceX + 1;
    const std::int32_t endY = MansionGrid::kEntranceY;
    exterior.traverseOuterWalls(pieces, data, baseGrid, StructureDirection::South, startX, startY, endX, endY);
    exterior.traverseOuterWalls(pieces, secondData, baseGrid, StructureDirection::South, startX, startY, endX, endY);

    // The tower: its wall starts at the first house cell found scanning rows from the north and
    // columns from the east, which is not the reading order and decides where its wall begins.
    MansionPlacement thirdData{
        .position = above(data.position, 19), .rotation = data.rotation, .wallType = "wall_window"};
    bool done = false;
    for (std::int32_t y = 0; y < thirdGrid.height() && !done; ++y) {
        for (std::int32_t x = thirdGrid.width() - 1; x >= 0; --x) {
            if (!MansionGrid::isHouse(thirdGrid, x, y)) {
                continue;
            }
            thirdData.position = relative(thirdData.position, rotateDirection(rotation, StructureDirection::South),
                                          8 + (y - startY) * 8);
            thirdData.position =
                relative(thirdData.position, rotateDirection(rotation, StructureDirection::East), (x - startX) * 8);
            exterior.wallPiece(pieces, thirdData);
            exterior.traverseOuterWalls(pieces, thirdData, thirdGrid, StructureDirection::South, x, y, x, y);
            done = true;
            break;
        }
    }

    exterior.createRoof(pieces, above(origin, 16), rotation, baseGrid, &thirdGrid);
    exterior.createRoof(pieces, above(origin, 27), rotation, thirdGrid, nullptr);

    for (std::size_t floor = 0; floor < 3; ++floor) {
        const StructurePos floorOrigin = above(origin, 8 * static_cast<std::int32_t>(floor) + (floor == 2 ? 3 : 0));
        const SimpleGrid& roomGrid = mansion.floorRooms(floor);
        const SimpleGrid& grid = floor == 2 ? thirdGrid : baseGrid;
        corridorFloor(pieces, floorOrigin, rotation, grid, roomGrid, floor, startX, startY);
        roomWalls(pieces, random, floorOrigin, rotation, mansion, grid, roomGrid, floor, startX, startY);
    }
}

} // namespace basalt::server::world::structure
