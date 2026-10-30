#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::world::structure {

/// parity: the block-writing half of `StructurePiece`.
class StructurePieceFill {
public:
    using ReplacePredicate = std::function<bool(std::int32_t)>;

    StructurePieceFill(StructureBoundingBox box, std::optional<StructureDirection> orientation, Chunk& chunk,
                       StructureBoundingBox clip, ReplacePredicate canBeReplaced = nullptr);

    [[nodiscard]] std::int32_t worldX(std::int32_t x, std::int32_t z) const;
    [[nodiscard]] std::int32_t worldY(std::int32_t y) const;
    [[nodiscard]] std::int32_t worldZ(std::int32_t x, std::int32_t z) const;

    [[nodiscard]] std::int32_t blockAt(std::int32_t x, std::int32_t y, std::int32_t z) const;

    void place(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) const;

    void placeRaw(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// parity: the structures that call `addFreshEntity` while placing.
    void placeEntity(const wire::common::Identifier& type, std::int32_t x, std::int32_t y, std::int32_t z,
                     wire::nbt::NbtCompound data, double yOffset = 0.5) const;

    [[nodiscard]] bool isInterior(std::int32_t x, std::int32_t y, std::int32_t z) const;

    void generateBox(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                     std::int32_t z1, std::int32_t edge, std::int32_t fill, bool skipAir) const;

    /// `generateBox` with a roll per cell. parity: `generateMaybeBox`, the roll runs for every cell.
    void generateMaybeBox(random::RandomSource& random, float probability, std::int32_t x0, std::int32_t y0,
                          std::int32_t z0, std::int32_t x1, std::int32_t y1, std::int32_t z1, std::int32_t edge,
                          std::int32_t fill, bool skipAir, bool onlyInside) const;

    /// parity: `generateUpperHalfSphere`.
    void generateUpperHalfSphere(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                                 std::int32_t z1, std::int32_t fill, bool skipAir) const;

    void maybePlace(random::RandomSource& random, float probability, std::int32_t x, std::int32_t y, std::int32_t z,
                    std::int32_t stateId) const;

private:
    /// parity: `StructurePiece.placeBlock` turns the state by the piece's orientation.
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
