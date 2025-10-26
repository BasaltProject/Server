#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::world::structure {

/// parity: the block-writing half of `StructurePiece`. A procedural piece is written in its own
/// coordinates and turned into world ones; every write is clipped to `clip`, which is how one piece
/// spans several chunks and is still written once per chunk. Templates go through `StructureTemplate`.
class StructurePieceFill {
public:
    /// A piece may refuse to overwrite what is already there; by default it does not.
    using ReplacePredicate = std::function<bool(std::int32_t)>;

    StructurePieceFill(StructureBoundingBox box, std::optional<StructureDirection> orientation, Chunk& chunk,
                       StructureBoundingBox clip, ReplacePredicate canBeReplaced = nullptr);

    [[nodiscard]] std::int32_t worldX(std::int32_t x, std::int32_t z) const;
    [[nodiscard]] std::int32_t worldY(std::int32_t y) const;
    [[nodiscard]] std::int32_t worldZ(std::int32_t x, std::int32_t z) const;

    /// The block already at a piece-local position, or air when it falls outside the clip.
    [[nodiscard]] std::int32_t blockAt(std::int32_t x, std::int32_t y, std::int32_t z) const;

    void place(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// Adds a generated entity at the centre of a piece-local block, if this chunk owns it.
    /// parity: the structures that call `addFreshEntity` while placing, such as the mineshaft's
    /// chest minecart, which rides the rail it just put down.
    void placeEntity(const wire::common::Identifier& type, std::int32_t x, std::int32_t y, std::int32_t z,
                     wire::nbt::NbtCompound data, double yOffset = 0.5) const;

    /// Whether the block above is still below the surface, so the piece is buried. parity:
    /// `StructurePiece.isInterior` reads `OCEAN_FLOOR_WG`; the chunk's own frozen map answers it,
    /// where a column function would give a lower floor wherever a structure raised the ground.
    [[nodiscard]] bool isInterior(std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// The shell of the box in `edge` and its inside in `fill`. With `skipAir` the cells that are
    /// already air are left alone, which is how a piece avoids walling itself into a cave.
    void generateBox(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                     std::int32_t z1, std::int32_t edge, std::int32_t fill, bool skipAir) const;

    /// `generateBox` with a roll per cell. parity: the roll happens for every cell before the other
    /// two conditions are even looked at, so the stream does not depend on the terrain.
    void generateMaybeBox(random::RandomSource& random, float probability, std::int32_t x0, std::int32_t y0,
                          std::int32_t z0, std::int32_t x1, std::int32_t y1, std::int32_t z1, std::int32_t edge,
                          std::int32_t fill, bool skipAir, bool onlyInside) const;

    /// parity: `generateUpperHalfSphere`. The dome a mineshaft room is roofed with: an ellipsoid
    /// measured from the floor up, so only its top half lands inside the box.
    void generateUpperHalfSphere(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                                 std::int32_t z1, std::int32_t fill, bool skipAir) const;

    void maybePlace(random::RandomSource& random, float probability, std::int32_t x, std::int32_t y, std::int32_t z,
                    std::int32_t stateId) const;

private:
    /// parity: `StructurePiece.placeBlock` turns the state by the piece's orientation, exactly as it
    /// turns the coordinates: a piece is written facing north and then rotated whole.
    [[nodiscard]] std::int32_t oriented(std::int32_t stateId) const;

    StructureBoundingBox box_;
    std::optional<StructureDirection> orientation_;
    Chunk* chunk_;
    StructureBoundingBox clip_;
    ReplacePredicate canBeReplaced_;
    StructureMirror mirror_ = StructureMirror::None;
    StructureRotation rotation_ = StructureRotation::None;
};

} // namespace basalt::server::world::structure
