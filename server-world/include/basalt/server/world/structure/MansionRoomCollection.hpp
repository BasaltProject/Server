#pragma once

#include <basalt/server/world/structure/MansionExterior.hpp>

#include <array>
#include <optional>
#include <string>

namespace basalt::server::world::structure {

/// parity: `WoodlandMansionPieces.FloorRoomCollection`. Which template goes in a room of each shape.
/// The third floor is the second floor's catalogue unchanged, exactly as `ThirdFloorRoomCollection`
/// inherits it without overriding anything.
class MansionRoomCollection {
public:
    explicit MansionRoomCollection(bool groundFloor) : groundFloor_(groundFloor) {}

    [[nodiscard]] std::string room1x1(random::RandomSource& random) const;
    [[nodiscard]] std::string room1x1Secret(random::RandomSource& random) const;
    [[nodiscard]] std::string room1x2SideEntrance(random::RandomSource& random, bool isStairs) const;
    [[nodiscard]] std::string room1x2FrontEntrance(random::RandomSource& random, bool isStairs) const;
    /// parity: the upper floors draw `nextInt(1)`, which always gives `1x2_se1` and still costs a roll.
    [[nodiscard]] std::string room1x2Secret(random::RandomSource& random) const;
    [[nodiscard]] std::string room2x2(random::RandomSource& random) const;
    [[nodiscard]] static std::string room2x2Secret();

    /// Ground floor, upper floor, tower. parity: the three collections, in that order.
    [[nodiscard]] static const std::array<MansionRoomCollection, 3>& byFloor();

private:
    bool groundFloor_;
};

/// parity: the four `addRoom*` methods of `MansionPiecePlacer`. Every combination of where the door
/// is and which way the room runs has its own offset, turn and mirror, and they are not derivable
/// from one another: the jar spells all sixteen out and so does this.
namespace mansionRoomFiller {

void addRoom1x1(std::vector<MansionPiece>& pieces, random::RandomSource& random, StructurePos position,
                StructureRotation rotation, std::optional<StructureDirection> doorDirection, std::size_t floor);

void addRoom1x2(std::vector<MansionPiece>& pieces, random::RandomSource& random, StructurePos position,
                StructureRotation rotation, std::optional<StructureDirection> roomDirection,
                StructureDirection doorDirection, std::size_t floor, bool isStairs);

void addRoom2x2(std::vector<MansionPiece>& pieces, random::RandomSource& random, StructurePos position,
                StructureRotation rotation, StructureDirection roomDirection, StructureDirection doorDirection,
                std::size_t floor);

void addRoom2x2Secret(std::vector<MansionPiece>& pieces, StructurePos position, StructureRotation rotation,
                      std::size_t floor);

} // namespace mansionRoomFiller

} // namespace basalt::server::world::structure
