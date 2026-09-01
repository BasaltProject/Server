#include <basalt/server/world/structure/MansionExterior.hpp>

namespace basalt::server::world::structure {
namespace {

[[nodiscard]] StructurePos above(StructurePos pos, std::int32_t distance) {
    return StructurePos{pos.x, pos.y + distance, pos.z};
}

} // namespace

StructureDirection rotateDirection(StructureRotation rotation, StructureDirection direction) {
    switch (rotation) {
    case StructureRotation::Clockwise90:
        return MansionGrid::clockWise(direction);
    case StructureRotation::Clockwise180:
        return opposite(direction);
    case StructureRotation::Counterclockwise90:
        return MansionGrid::counterClockWise(direction);
    case StructureRotation::None:
    default:
        return direction;
    }
}

void MansionExterior::entrance(std::vector<MansionPiece>& pieces, MansionPlacement& data) const {
    const StructureDirection west = rotateDirection(data.rotation, StructureDirection::West);
    pieces.push_back(MansionPiece{
        .templateName = "entrance", .position = relative(data.position, west, 9), .rotation = data.rotation});
    data.position = relative(data.position, rotateDirection(data.rotation, StructureDirection::South), 16);
}

void MansionExterior::wallPiece(std::vector<MansionPiece>& pieces, MansionPlacement& data) const {
    pieces.push_back(
        MansionPiece{.templateName = data.wallType,
                     .position = relative(data.position, rotateDirection(data.rotation, StructureDirection::East), 7),
                     .rotation = data.rotation});
    data.position = relative(data.position, rotateDirection(data.rotation, StructureDirection::South), 8);
}

void MansionExterior::turn(std::vector<MansionPiece>& pieces, MansionPlacement& data) const {
    data.position = relative(data.position, rotateDirection(data.rotation, StructureDirection::South), -1);
    pieces.push_back(MansionPiece{.templateName = "wall_corner", .position = data.position, .rotation = data.rotation});
    data.position = relative(data.position, rotateDirection(data.rotation, StructureDirection::South), -7);
    data.position = relative(data.position, rotateDirection(data.rotation, StructureDirection::West), -6);
    data.rotation = rotated(data.rotation, StructureRotation::Clockwise90);
}

void MansionExterior::innerTurn(MansionPlacement& data) const {
    data.position = relative(data.position, rotateDirection(data.rotation, StructureDirection::South), 6);
    data.position = relative(data.position, rotateDirection(data.rotation, StructureDirection::East), 8);
    data.rotation = rotated(data.rotation, StructureRotation::Counterclockwise90);
}

void MansionExterior::traverseOuterWalls(std::vector<MansionPiece>& pieces, MansionPlacement& data,
                                         const SimpleGrid& grid, StructureDirection heading, std::int32_t startGridX,
                                         std::int32_t startGridY, std::int32_t endX, std::int32_t endY) const {
    std::int32_t gridX = startGridX;
    std::int32_t gridY = startGridY;
    StructureDirection direction = heading;
    const StructureDirection startDirection = heading;
    do {
        const bool aheadIsHouse = MansionGrid::isHouse(grid, gridX + dx(direction), gridY + dz(direction));
        const StructureDirection ccw = MansionGrid::counterClockWise(direction);
        if (!aheadIsHouse) {
            turn(pieces, data);
            direction = MansionGrid::clockWise(direction);
            if (gridX != endX || gridY != endY || startDirection != direction) {
                wallPiece(pieces, data);
            }
        } else if (MansionGrid::isHouse(grid, gridX + dx(direction) + dx(ccw), gridY + dz(direction) + dz(ccw))) {
            innerTurn(data);
            gridX += dx(direction);
            gridY += dz(direction);
            direction = ccw;
        } else {
            gridX += dx(direction);
            gridY += dz(direction);
            if (gridX != endX || gridY != endY || startDirection != direction) {
                wallPiece(pieces, data);
            }
        }
    } while (gridX != endX || gridY != endY || startDirection != direction);
}

StructurePos MansionExterior::cellPosition(StructurePos origin, StructureRotation rotation, std::int32_t x,
                                           std::int32_t y) const {
    StructurePos position =
        relative(origin, rotateDirection(rotation, StructureDirection::South), 8 + (y - startY_) * 8);
    return relative(position, rotateDirection(rotation, StructureDirection::East), (x - startX_) * 8);
}

void MansionExterior::createRoof(std::vector<MansionPiece>& pieces, StructurePos roofOrigin, StructureRotation rotation,
                                 const SimpleGrid& grid, const SimpleGrid* aboveGrid) const {
    for (std::int32_t y = 0; y < grid.height(); ++y) {
        for (std::int32_t x = 0; x < grid.width(); ++x) {
            const StructurePos position = cellPosition(roofOrigin, rotation, x, y);
            const bool isAbove = aboveGrid != nullptr && MansionGrid::isHouse(*aboveGrid, x, y);
            if (!MansionGrid::isHouse(grid, x, y) || isAbove) {
                continue;
            }
            pieces.push_back(
                MansionPiece{.templateName = "roof", .position = above(position, 3), .rotation = rotation});
            if (!MansionGrid::isHouse(grid, x + 1, y)) {
                pieces.push_back(
                    MansionPiece{.templateName = "roof_front",
                                 .position = relative(position, rotateDirection(rotation, StructureDirection::East), 6),
                                 .rotation = rotation});
            }
            if (!MansionGrid::isHouse(grid, x - 1, y)) {
                pieces.push_back(MansionPiece{
                    .templateName = "roof_front",
                    .position = relative(position, rotateDirection(rotation, StructureDirection::South), 7),
                    .rotation = rotated(rotation, StructureRotation::Clockwise180)});
            }
            if (!MansionGrid::isHouse(grid, x, y - 1)) {
                pieces.push_back(
                    MansionPiece{.templateName = "roof_front",
                                 .position = relative(position, rotateDirection(rotation, StructureDirection::West), 1),
                                 .rotation = rotated(rotation, StructureRotation::Counterclockwise90)});
            }
            if (!MansionGrid::isHouse(grid, x, y + 1)) {
                StructurePos corner = relative(position, rotateDirection(rotation, StructureDirection::East), 6);
                corner = relative(corner, rotateDirection(rotation, StructureDirection::South), 6);
                pieces.push_back(MansionPiece{.templateName = "roof_front",
                                              .position = corner,
                                              .rotation = rotated(rotation, StructureRotation::Clockwise90)});
            }
        }
    }

    if (aboveGrid != nullptr) {
        for (std::int32_t y = 0; y < grid.height(); ++y) {
            for (std::int32_t x = 0; x < grid.width(); ++x) {
                const StructurePos position = cellPosition(roofOrigin, rotation, x, y);
                if (!MansionGrid::isHouse(grid, x, y) || !MansionGrid::isHouse(*aboveGrid, x, y)) {
                    continue;
                }
                smallWalls(pieces, position, rotation, grid, x, y);
            }
        }
    }

    for (std::int32_t y = 0; y < grid.height(); ++y) {
        for (std::int32_t x = 0; x < grid.width(); ++x) {
            const StructurePos position = cellPosition(roofOrigin, rotation, x, y);
            const bool isAbove = aboveGrid != nullptr && MansionGrid::isHouse(*aboveGrid, x, y);
            if (!MansionGrid::isHouse(grid, x, y) || isAbove) {
                continue;
            }
            roofCorners(pieces, position, rotation, grid, x, y);
        }
    }
}

void MansionExterior::smallWalls(std::vector<MansionPiece>& pieces, StructurePos position, StructureRotation rotation,
                                 const SimpleGrid& grid, std::int32_t x, std::int32_t y) const {
    const StructureDirection east = rotateDirection(rotation, StructureDirection::East);
    const StructureDirection west = rotateDirection(rotation, StructureDirection::West);
    const StructureDirection north = rotateDirection(rotation, StructureDirection::North);
    const StructureDirection south = rotateDirection(rotation, StructureDirection::South);
    if (!MansionGrid::isHouse(grid, x + 1, y)) {
        pieces.push_back(
            MansionPiece{.templateName = "small_wall", .position = relative(position, east, 7), .rotation = rotation});
    }
    if (!MansionGrid::isHouse(grid, x - 1, y)) {
        pieces.push_back(MansionPiece{.templateName = "small_wall",
                                      .position = relative(relative(position, west, 1), south, 6),
                                      .rotation = rotated(rotation, StructureRotation::Clockwise180)});
    }
    if (!MansionGrid::isHouse(grid, x, y - 1)) {
        pieces.push_back(MansionPiece{.templateName = "small_wall",
                                      .position = relative(relative(position, west, 0), north, 1),
                                      .rotation = rotated(rotation, StructureRotation::Counterclockwise90)});
    }
    if (!MansionGrid::isHouse(grid, x, y + 1)) {
        pieces.push_back(MansionPiece{.templateName = "small_wall",
                                      .position = relative(relative(position, east, 6), south, 7),
                                      .rotation = rotated(rotation, StructureRotation::Clockwise90)});
    }
    if (!MansionGrid::isHouse(grid, x + 1, y)) {
        if (!MansionGrid::isHouse(grid, x, y - 1)) {
            pieces.push_back(MansionPiece{.templateName = "small_wall_corner",
                                          .position = relative(relative(position, east, 7), north, 2),
                                          .rotation = rotation});
        }
        if (!MansionGrid::isHouse(grid, x, y + 1)) {
            pieces.push_back(MansionPiece{.templateName = "small_wall_corner",
                                          .position = relative(relative(position, east, 8), south, 7),
                                          .rotation = rotated(rotation, StructureRotation::Clockwise90)});
        }
    }
    if (!MansionGrid::isHouse(grid, x - 1, y)) {
        if (!MansionGrid::isHouse(grid, x, y - 1)) {
            pieces.push_back(MansionPiece{.templateName = "small_wall_corner",
                                          .position = relative(relative(position, west, 2), north, 1),
                                          .rotation = rotated(rotation, StructureRotation::Counterclockwise90)});
        }
        if (!MansionGrid::isHouse(grid, x, y + 1)) {
            pieces.push_back(MansionPiece{.templateName = "small_wall_corner",
                                          .position = relative(relative(position, west, 1), south, 8),
                                          .rotation = rotated(rotation, StructureRotation::Clockwise180)});
        }
    }
}

void MansionExterior::roofCorners(std::vector<MansionPiece>& pieces, StructurePos position, StructureRotation rotation,
                                  const SimpleGrid& grid, std::int32_t x, std::int32_t y) const {
    const StructureDirection east = rotateDirection(rotation, StructureDirection::East);
    const StructureDirection west = rotateDirection(rotation, StructureDirection::West);
    const StructureDirection north = rotateDirection(rotation, StructureDirection::North);
    const StructureDirection south = rotateDirection(rotation, StructureDirection::South);
    if (!MansionGrid::isHouse(grid, x + 1, y)) {
        const StructurePos corner = relative(position, east, 6);
        if (!MansionGrid::isHouse(grid, x, y + 1)) {
            pieces.push_back(MansionPiece{
                .templateName = "roof_corner", .position = relative(corner, south, 6), .rotation = rotation});
        } else if (MansionGrid::isHouse(grid, x + 1, y + 1)) {
            pieces.push_back(MansionPiece{
                .templateName = "roof_inner_corner", .position = relative(corner, south, 5), .rotation = rotation});
        }
        if (!MansionGrid::isHouse(grid, x, y - 1)) {
            pieces.push_back(MansionPiece{.templateName = "roof_corner",
                                          .position = corner,
                                          .rotation = rotated(rotation, StructureRotation::Counterclockwise90)});
        } else if (MansionGrid::isHouse(grid, x + 1, y - 1)) {
            pieces.push_back(MansionPiece{.templateName = "roof_inner_corner",
                                          .position = relative(relative(position, east, 9), north, 2),
                                          .rotation = rotated(rotation, StructureRotation::Clockwise90)});
        }
    }
    if (!MansionGrid::isHouse(grid, x - 1, y)) {
        if (!MansionGrid::isHouse(grid, x, y + 1)) {
            pieces.push_back(MansionPiece{.templateName = "roof_corner",
                                          .position = relative(position, south, 6),
                                          .rotation = rotated(rotation, StructureRotation::Clockwise90)});
        } else if (MansionGrid::isHouse(grid, x - 1, y + 1)) {
            pieces.push_back(MansionPiece{.templateName = "roof_inner_corner",
                                          .position = relative(relative(position, south, 8), west, 3),
                                          .rotation = rotated(rotation, StructureRotation::Counterclockwise90)});
        }
        if (!MansionGrid::isHouse(grid, x, y - 1)) {
            pieces.push_back(MansionPiece{.templateName = "roof_corner",
                                          .position = position,
                                          .rotation = rotated(rotation, StructureRotation::Clockwise180)});
        } else if (MansionGrid::isHouse(grid, x - 1, y - 1)) {
            pieces.push_back(MansionPiece{.templateName = "roof_inner_corner",
                                          .position = relative(position, south, 1),
                                          .rotation = rotated(rotation, StructureRotation::Clockwise180)});
        }
    }
}

} // namespace basalt::server::world::structure
