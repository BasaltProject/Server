#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePieceFill.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace basalt::server::world::terrain {

/// parity: `JungleTemplePiece`.
class JungleTemplePiece {
public:
    static constexpr std::int32_t kWidth = 12;
    static constexpr std::int32_t kDepth = 15;
    static constexpr std::int32_t kHeight = 10;

    JungleTemplePiece(std::int32_t west, std::int32_t north, structure::StructureDirection orientation);

    [[nodiscard]] const structure::StructureBoundingBox& bounds() const noexcept { return bounds_; }

    /// parity: `updateAverageGroundHeight` over `MOTION_BLOCKING_NO_LEAVES`.
    bool settleOnGround(const std::function<std::int32_t(std::int32_t, std::int32_t)>& groundAt);

    void place(Chunk& chunk, const structure::StructureBoundingBox& clip, random::RandomSource& random);

private:
    void buildShell() const;
    void buildRoof() const;
    void buildStairs() const;
    void buildCellar() const;
    void buildTripwireTrap() const;
    void buildArrowTrap() const;
    void buildLeverPuzzle() const;

    /// parity: `generateBox` with the moss stone selector.
    void mossBox(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                 std::int32_t z1) const;
    void airBox(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                std::int32_t z1) const;

    void createChest(std::int32_t x, std::int32_t y, std::int32_t z) const;
    void createDispenser(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view facing) const;
    void putLoot(std::int32_t worldX, std::int32_t worldY, std::int32_t worldZ, std::string_view type,
                 std::string_view table, std::int64_t seed) const;

    [[nodiscard]] std::string transformedFacing(std::string_view facing) const;

    void set(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) const;

    structure::StructureDirection orientation_;
    structure::StructureBoundingBox bounds_;
    structure::StructureMirror mirror_;
    structure::StructureRotation rotation_;
    Chunk* chunk_ = nullptr;
    structure::StructureBoundingBox clip_;
    random::RandomSource* random_ = nullptr;
    std::unique_ptr<structure::StructurePieceFill> fill_;
};

} // namespace basalt::server::world::terrain
