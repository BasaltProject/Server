#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePieceFill.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace basalt::server::world::terrain {

/// parity: `DesertPyramidPiece`. Twenty-one blocks square, so unlike the swamp hut it spills into its
/// neighbours and the clip decides what lands, as vanilla's `chunkBB` does. The suspicious sand is
/// not written here: `suspiciousSandPositions` only collects candidates.
class DesertPyramidPiece {
public:
    static constexpr std::int32_t kWidth = 21;
    static constexpr std::int32_t kDepth = 21;
    static constexpr std::int32_t kHeight = 15;

    DesertPyramidPiece(std::int32_t west, std::int32_t north, structure::StructureDirection orientation,
                       std::int32_t groundOffset);

    [[nodiscard]] const structure::StructureBoundingBox& bounds() const noexcept { return bounds_; }

    /// World positions vanilla records as candidates for suspicious sand, in its own order.
    [[nodiscard]] const std::vector<structure::StructurePos>& suspiciousSandPositions() const noexcept {
        return suspiciousSand_;
    }

    /// The one collapsed-roof block that is always suspicious, whichever else is picked.
    [[nodiscard]] const std::optional<structure::StructurePos>& collapsedRoofPos() const noexcept {
        return collapsedRoof_;
    }

    /// parity: `updateHeightPositionToLowestGroundHeight`, the lowest ground under the whole
    /// footprint rather than the mean the hut uses, then dropped by up to two more.
    bool settleOnGround(const std::function<std::int32_t(std::int32_t, std::int32_t)>& groundAt);

    void place(Chunk& chunk, const structure::StructureBoundingBox& clip, std::int64_t worldSeed,
               random::RandomSource& random);

private:
    void buildShell() const;
    void buildTowers() const;
    void buildEntrance() const;
    void buildChambers() const;
    void buildFloorMosaic() const;
    void buildWallPattern() const;
    void buildTrapChamber() const;
    void placeChests(random::RandomSource& random) const;
    void addCellarStairs() const;
    void addCellarRoom();

    /// parity: `placeCollapsedRoof`. The material is another draw off the level's own random, so it
    /// gets the same positional stand-in as the ramp; the block that is always suspicious is not,
    /// and comes off the world seed exactly as vanilla forks it.
    void placeCollapsedRoof(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t z1);

    void recordSandBox(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                       std::int32_t z1);

    /// parity: `placeSand`, which writes nothing: it only offers the position to the sand pass.
    void recordSand(std::int32_t x, std::int32_t y, std::int32_t z);

    void createChest(random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) const;

    void fillColumnDown(std::int32_t stateId, std::int32_t x, std::int32_t startY, std::int32_t z) const;

    /// A per-position stand-in for vanilla's level random, which no seed can reproduce.
    [[nodiscard]] random::RandomSource positionalAt(std::int32_t x, std::int32_t y, std::int32_t z) const;

    void set(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) const;
    void box(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1, std::int32_t z1,
             std::int32_t edge, std::int32_t fill) const;

    structure::StructureDirection orientation_;
    std::int32_t groundOffset_;
    structure::StructureBoundingBox bounds_;
    std::vector<structure::StructurePos> suspiciousSand_;
    std::optional<structure::StructurePos> collapsedRoof_;
    Chunk* chunk_ = nullptr;
    structure::StructureBoundingBox clip_;
    std::int64_t worldSeed_ = 0;
    std::unique_ptr<structure::StructurePieceFill> fill_;
};

} // namespace basalt::server::world::terrain
