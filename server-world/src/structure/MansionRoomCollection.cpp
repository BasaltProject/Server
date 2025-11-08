#include <basalt/server/world/structure/MansionRoomCollection.hpp>

namespace basalt::server::world::structure {

std::string MansionRoomCollection::room1x1(random::RandomSource& random) const {
    return (groundFloor_ ? "1x1_a" : "1x1_b") + std::to_string(random.nextInt(5) + 1);
}

std::string MansionRoomCollection::room1x1Secret(random::RandomSource& random) const {
    return "1x1_as" + std::to_string(random.nextInt(4) + 1);
}

std::string MansionRoomCollection::room1x2SideEntrance(random::RandomSource& random, bool isStairs) const {
    if (groundFloor_) {
        return "1x2_a" + std::to_string(random.nextInt(9) + 1);
    }
    return isStairs ? "1x2_c_stairs" : "1x2_c" + std::to_string(random.nextInt(4) + 1);
}

std::string MansionRoomCollection::room1x2FrontEntrance(random::RandomSource& random, bool isStairs) const {
    if (groundFloor_) {
        return "1x2_b" + std::to_string(random.nextInt(5) + 1);
    }
    return isStairs ? "1x2_d_stairs" : "1x2_d" + std::to_string(random.nextInt(5) + 1);
}

std::string MansionRoomCollection::room1x2Secret(random::RandomSource& random) const {
    return groundFloor_ ? "1x2_s" + std::to_string(random.nextInt(2) + 1)
                        : "1x2_se" + std::to_string(random.nextInt(1) + 1);
}

std::string MansionRoomCollection::room2x2(random::RandomSource& random) const {
    return (groundFloor_ ? "2x2_a" : "2x2_b") + std::to_string(random.nextInt(groundFloor_ ? 4 : 5) + 1);
}

std::string MansionRoomCollection::room2x2Secret() { return "2x2_s1"; }

const std::array<MansionRoomCollection, 3>& MansionRoomCollection::byFloor() {
    static const std::array<MansionRoomCollection, 3> built{MansionRoomCollection{true}, MansionRoomCollection{false},
                                                            MansionRoomCollection{false}};
    return built;
}

void mansionRoomFiller::addRoom1x1(std::vector<MansionPiece>& pieces, random::RandomSource& random,
                                   StructurePos position, StructureRotation rotation,
                                   std::optional<StructureDirection> doorDirection, std::size_t floor) {
    const MansionRoomCollection& rooms = MansionRoomCollection::byFloor()[floor];
    // The draw for an ordinary room happens first and is thrown away when the door turns out to be
    // the secret one, so a secret room costs two rolls, not one.
    std::string templateName = rooms.room1x1(random);
    StructureRotation pieceRotation = StructureRotation::None;
    if (doorDirection != StructureDirection::East) {
        if (doorDirection == StructureDirection::North) {
            pieceRotation = rotated(pieceRotation, StructureRotation::Counterclockwise90);
        } else if (doorDirection == StructureDirection::West) {
            pieceRotation = rotated(pieceRotation, StructureRotation::Clockwise180);
        } else if (doorDirection == StructureDirection::South) {
            pieceRotation = rotated(pieceRotation, StructureRotation::Clockwise90);
        } else {
            templateName = rooms.room1x1Secret(random);
        }
    }
    StructurePos zero = zeroPositionWithTransform(StructurePos{1, 0, 0}, StructureMirror::None, pieceRotation, 7, 7);
    pieceRotation = rotated(pieceRotation, rotation);
    zero = rotate(zero, rotation);
    pieces.push_back(MansionPiece{.templateName = std::move(templateName),
                                  .position = StructurePos{position.x + zero.x, position.y, position.z + zero.z},
                                  .rotation = pieceRotation});
}

void mansionRoomFiller::addRoom1x2(std::vector<MansionPiece>& pieces, random::RandomSource& random,
                                   StructurePos position, StructureRotation rotation,
                                   std::optional<StructureDirection> roomDirection, StructureDirection doorDirection,
                                   std::size_t floor, bool isStairs) {
    const MansionRoomCollection& rooms = MansionRoomCollection::byFloor()[floor];
    const StructureDirection east = rotateDirection(rotation, StructureDirection::East);
    const StructureDirection west = rotateDirection(rotation, StructureDirection::West);
    const StructureDirection north = rotateDirection(rotation, StructureDirection::North);
    const StructureDirection south = rotateDirection(rotation, StructureDirection::South);
    const StructureDirection d = doorDirection;
    const std::optional<StructureDirection> r = roomDirection;
    const auto add = [&pieces](std::string name, StructurePos at, StructureRotation turn,
                               StructureMirror mirror = StructureMirror::None) {
        pieces.push_back(
            MansionPiece{.templateName = std::move(name), .position = at, .rotation = turn, .mirror = mirror});
    };

    if (d == StructureDirection::East && r == StructureDirection::South) {
        add(rooms.room1x2SideEntrance(random, isStairs), relative(position, east, 1), rotation);
    } else if (d == StructureDirection::East && r == StructureDirection::North) {
        add(rooms.room1x2SideEntrance(random, isStairs), relative(relative(position, east, 1), south, 6), rotation,
            StructureMirror::LeftRight);
    } else if (d == StructureDirection::West && r == StructureDirection::North) {
        add(rooms.room1x2SideEntrance(random, isStairs), relative(relative(position, east, 7), south, 6),
            rotated(rotation, StructureRotation::Clockwise180));
    } else if (d == StructureDirection::West && r == StructureDirection::South) {
        add(rooms.room1x2SideEntrance(random, isStairs), relative(position, east, 7), rotation,
            StructureMirror::FrontBack);
    } else if (d == StructureDirection::South && r == StructureDirection::East) {
        add(rooms.room1x2SideEntrance(random, isStairs), relative(position, east, 1),
            rotated(rotation, StructureRotation::Clockwise90), StructureMirror::LeftRight);
    } else if (d == StructureDirection::South && r == StructureDirection::West) {
        add(rooms.room1x2SideEntrance(random, isStairs), relative(position, east, 7),
            rotated(rotation, StructureRotation::Clockwise90));
    } else if (d == StructureDirection::North && r == StructureDirection::West) {
        add(rooms.room1x2SideEntrance(random, isStairs), relative(relative(position, east, 7), south, 6),
            rotated(rotation, StructureRotation::Clockwise90), StructureMirror::FrontBack);
    } else if (d == StructureDirection::North && r == StructureDirection::East) {
        add(rooms.room1x2SideEntrance(random, isStairs), relative(relative(position, east, 1), south, 6),
            rotated(rotation, StructureRotation::Counterclockwise90));
    } else if (d == StructureDirection::South && r == StructureDirection::North) {
        add(rooms.room1x2FrontEntrance(random, isStairs), relative(relative(position, east, 1), north, 8), rotation);
    } else if (d == StructureDirection::North && r == StructureDirection::South) {
        add(rooms.room1x2FrontEntrance(random, isStairs), relative(relative(position, east, 7), south, 14),
            rotated(rotation, StructureRotation::Clockwise180));
    } else if (d == StructureDirection::West && r == StructureDirection::East) {
        add(rooms.room1x2FrontEntrance(random, isStairs), relative(position, east, 15),
            rotated(rotation, StructureRotation::Clockwise90));
    } else if (d == StructureDirection::East && r == StructureDirection::West) {
        add(rooms.room1x2FrontEntrance(random, isStairs), relative(relative(position, west, 7), south, 6),
            rotated(rotation, StructureRotation::Counterclockwise90));
    } else if (d == StructureDirection::Up && r == StructureDirection::East) {
        add(rooms.room1x2Secret(random), relative(position, east, 15),
            rotated(rotation, StructureRotation::Clockwise90));
    } else if (d == StructureDirection::Up && r == StructureDirection::South) {
        add(rooms.room1x2Secret(random), relative(position, east, 1), rotation);
    }
}

void mansionRoomFiller::addRoom2x2(std::vector<MansionPiece>& pieces, random::RandomSource& random,
                                   StructurePos position, StructureRotation rotation, StructureDirection roomDirection,
                                   StructureDirection doorDirection, std::size_t floor) {
    std::int32_t east = 0;
    std::int32_t south = 0;
    StructureRotation pieceRotation = rotation;
    StructureMirror mirror = StructureMirror::None;
    const StructureDirection d = doorDirection;
    const StructureDirection r = roomDirection;
    if (d == StructureDirection::East && r == StructureDirection::South) {
        east = -7;
    } else if (d == StructureDirection::East && r == StructureDirection::North) {
        east = -7;
        south = 6;
        mirror = StructureMirror::LeftRight;
    } else if (d == StructureDirection::North && r == StructureDirection::East) {
        east = 1;
        south = 14;
        pieceRotation = rotated(rotation, StructureRotation::Counterclockwise90);
    } else if (d == StructureDirection::North && r == StructureDirection::West) {
        east = 7;
        south = 14;
        pieceRotation = rotated(rotation, StructureRotation::Counterclockwise90);
        mirror = StructureMirror::LeftRight;
    } else if (d == StructureDirection::South && r == StructureDirection::West) {
        east = 7;
        south = -8;
        pieceRotation = rotated(rotation, StructureRotation::Clockwise90);
    } else if (d == StructureDirection::South && r == StructureDirection::East) {
        east = 1;
        south = -8;
        pieceRotation = rotated(rotation, StructureRotation::Clockwise90);
        mirror = StructureMirror::LeftRight;
    } else if (d == StructureDirection::West && r == StructureDirection::North) {
        east = 15;
        south = 6;
        pieceRotation = rotated(rotation, StructureRotation::Clockwise180);
    } else if (d == StructureDirection::West && r == StructureDirection::South) {
        east = 15;
        mirror = StructureMirror::FrontBack;
    }
    StructurePos target = relative(position, rotateDirection(rotation, StructureDirection::East), east);
    target = relative(target, rotateDirection(rotation, StructureDirection::South), south);
    pieces.push_back(MansionPiece{.templateName = MansionRoomCollection::byFloor()[floor].room2x2(random),
                                  .position = target,
                                  .rotation = pieceRotation,
                                  .mirror = mirror});
}

void mansionRoomFiller::addRoom2x2Secret(std::vector<MansionPiece>& pieces, StructurePos position,
                                         StructureRotation rotation, std::size_t floor) {
    static_cast<void>(floor);
    const StructurePos target = relative(position, rotateDirection(rotation, StructureDirection::East), 1);
    pieces.push_back(MansionPiece{.templateName = MansionRoomCollection::room2x2Secret(),
                                  .position = target,
                                  .rotation = rotation,
                                  .mirror = StructureMirror::None});
}

} // namespace basalt::server::world::structure
