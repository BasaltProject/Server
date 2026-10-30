#pragma once

#include <basalt/server/world/structure/MansionGrid.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::world::structure {

/// One template of a mansion, ready to be placed. parity: `WoodlandMansionPiece`.
struct MansionPiece {
    std::string templateName;
    StructurePos position;
    StructureRotation rotation = StructureRotation::None;
    StructureMirror mirror = StructureMirror::None;

    friend bool operator==(const MansionPiece&, const MansionPiece&) = default;
};

/// parity: `WoodlandMansionPieces.PlacementData`.
struct MansionPlacement {
    StructurePos position;
    StructureRotation rotation = StructureRotation::None;
    std::string wallType;
};

/// parity: the wall and roof half of `WoodlandMansionPieces.MansionPiecePlacer`.
class MansionExterior {
public:
    MansionExterior(std::int32_t startX, std::int32_t startY) : startX_(startX), startY_(startY) {}

    void entrance(std::vector<MansionPiece>& pieces, MansionPlacement& data) const;
    void wallPiece(std::vector<MansionPiece>& pieces, MansionPlacement& data) const;
    void turn(std::vector<MansionPiece>& pieces, MansionPlacement& data) const;
    void innerTurn(MansionPlacement& data) const;

    /// Walks the outer walls. parity: `traverseOuterWalls`, a do-while, so a one-cell house still runs once.
    void traverseOuterWalls(std::vector<MansionPiece>& pieces, MansionPlacement& data, const SimpleGrid& grid,
                            StructureDirection heading, std::int32_t startGridX, std::int32_t startGridY,
                            std::int32_t endX, std::int32_t endY) const;

    /// parity: `createRoof`.
    void createRoof(std::vector<MansionPiece>& pieces, StructurePos roofOrigin, StructureRotation rotation,
                    const SimpleGrid& grid, const SimpleGrid* aboveGrid) const;

    [[nodiscard]] StructurePos cellPosition(StructurePos origin, StructureRotation rotation, std::int32_t x,
                                            std::int32_t y) const;

private:
    void smallWalls(std::vector<MansionPiece>& pieces, StructurePos position, StructureRotation rotation,
                    const SimpleGrid& grid, std::int32_t x, std::int32_t y) const;
    void roofCorners(std::vector<MansionPiece>& pieces, StructurePos position, StructureRotation rotation,
                     const SimpleGrid& grid, std::int32_t x, std::int32_t y) const;

    std::int32_t startX_;
    std::int32_t startY_;
};

/// parity: `Rotation.rotate(Direction)`. The vertical faces are left alone.
[[nodiscard]] StructureDirection rotateDirection(StructureRotation rotation, StructureDirection direction);

} // namespace basalt::server::world::structure
