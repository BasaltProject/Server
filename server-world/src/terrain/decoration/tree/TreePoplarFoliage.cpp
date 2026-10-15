#include <basalt/server/world/terrain/decoration/tree/TreeFoliagePlacers.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeHelpers.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace basalt::server::world::terrain::decoration::tree {
namespace {

/// parity: `PoplarFoliagePlacer.shouldRowBePartialRhombusShape`, the top two rows.
[[nodiscard]] bool isPartialRow(std::int32_t foliageHeight, std::int32_t y) {
    return foliageHeight - 1 == y || foliageHeight - 2 == y;
}

/// parity: `PoplarFoliagePlacer.getCornerBlocksToCutForRhombusShape`.
[[nodiscard]] std::int32_t cornerCut(std::int32_t dx, std::int32_t dz, std::int32_t radius, bool partial, bool flip) {
    const bool smallCorner = flip ? (dx > 0 && dz > 0) || (dz < 0 && dx < 0) : (dx > 0 && dz < 0) || (dz > 0 && dx < 0);
    if (smallCorner) {
        return radius - 1;
    }
    return partial ? radius + 1 : radius;
}

[[nodiscard]] bool withinRhombus(std::int32_t radius, std::int32_t absDx, std::int32_t absDz, std::int32_t cut,
                                 std::int32_t extra) {
    return absDx + absDz <= radius * 2 - (cut + extra);
}

} // namespace

void FoliagePlacer::createPoplarFoliage(Chunk& chunk, random::RandomSource& random, const std::int32_t foliageStateId,
                                        const std::int32_t trunkStateId, const FoliageAttachment& attachment,
                                        const std::int32_t foliageHeight, const std::int32_t leafRadius,
                                        const std::int32_t offset, FoliagePositions& foliagePositions) const {
    const std::int32_t originX = attachment.x;
    const std::int32_t originY = attachment.y + offset;
    const std::int32_t originZ = attachment.z;
    const std::int32_t extra = attachment.doubleTrunk ? 1 : 0;
    const std::int32_t radius = leafRadius + attachment.radiusOffset - 1;
    const bool flip = random.nextBoolean();

    const auto row = [&](std::int32_t currentRadius, std::int32_t y) {
        const bool partial = isPartialRow(foliageHeight, y);
        for (std::int32_t dx = -currentRadius; dx <= currentRadius + extra; ++dx) {
            for (std::int32_t dz = -currentRadius; dz <= currentRadius + extra; ++dz) {
                const std::int32_t absDx = std::abs(dx);
                const std::int32_t absDz = std::abs(dz);
                // parity: the edge of a partial row is cut before the side hole is rolled.
                if (partial && (absDx == currentRadius || absDz == currentRadius)) {
                    continue;
                }
                const std::int32_t hole = random.nextFloat() <= sideHoleChance_ ? 1 : 0;
                if (!withinRhombus(currentRadius, absDx, absDz, cornerCut(dx, dz, currentRadius, partial, flip),
                                   hole)) {
                    continue;
                }
                const std::int32_t x = originX + dx;
                const std::int32_t leafY = originY + y;
                const std::int32_t z = originZ + dz;
                if (tryPlaceLeaf(chunk, foliageStateId, x, leafY, z)) {
                    foliagePositions.emplace(x, leafY, z);
                }
            }
        }
    };

    row(radius - 2, foliageHeight - 1);
    row(radius - 1, foliageHeight - 2);
    row(radius - 1, foliageHeight - 3);
    for (std::int32_t y = foliageHeight - 4; y >= 1; --y) {
        row(radius, y);
    }

    // parity: `replaceLeavesWithLog`.
    const std::int32_t coreY = foliageHeight - 4;
    const bool corePartial = isPartialRow(foliageHeight, coreY);
    for (std::int32_t dx = -radius; dx <= radius + extra; ++dx) {
        for (std::int32_t dz = -radius; dz <= radius + extra; ++dz) {
            const std::int32_t absDx = std::abs(dx);
            const std::int32_t absDz = std::abs(dz);
            if (!withinRhombus(radius, absDx, absDz, cornerCut(dx, dz, radius, corePartial, flip), 2) ||
                !((absDz == 0 && radius - absDx >= 4) || (absDx == 0 && radius - absDz >= 4))) {
                continue;
            }
            const std::int32_t x = originX + dx;
            const std::int32_t y = originY + coreY;
            const std::int32_t z = originZ + dz;
            const ChunkHeight bounds = chunk.verticalBounds();
            if (y < bounds.minY() || y > bounds.maxY() || chunk.decorationBlockStateId(x, y, z) != foliageStateId) {
                continue;
            }
            chunk.setDecorationBlockStateId(x, y, z, absDz == 0 ? trunkStateId - 1 : trunkStateId + 1);
            foliagePositions.emplace(x, y, z);
        }
    }

    row(radius - 1, 0);
    row(std::clamp(radius - 2, 1, 2), -1);
}

} // namespace basalt::server::world::terrain::decoration::tree
