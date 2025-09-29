#include <basalt/server/world/terrain/decoration/tree/TreeFoliagePlacers.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeHelpers.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace basalt::server::world::terrain::decoration::tree {
namespace {

/// parity: real `Direction.Plane.HORIZONTAL` face order (NORTH, EAST, SOUTH, WEST) with each
/// direction's own `getClockWise` as the edge normal, and EAST/SOUTH the positive axes.
constexpr std::array<HorizontalDirection, 4> kAlongEdges{
    HorizontalDirection{.dx = 0, .dz = -1}, HorizontalDirection{.dx = 1, .dz = 0},
    HorizontalDirection{.dx = 0, .dz = 1}, HorizontalDirection{.dx = -1, .dz = 0}};
constexpr std::array<HorizontalDirection, 4> kToEdges{
    HorizontalDirection{.dx = 1, .dz = 0}, HorizontalDirection{.dx = 0, .dz = 1},
    HorizontalDirection{.dx = -1, .dz = 0}, HorizontalDirection{.dx = 0, .dz = -1}};
constexpr std::array<bool, 4> kToEdgePositive{true, true, false, false};

} // namespace

FoliagePlacer FoliagePlacer::blob(IntProvider radius, IntProvider offset, const std::int32_t height) {
    FoliagePlacer placer;
    placer.kind_ = Kind::Blob;
    placer.radius_ = std::move(radius);
    placer.offset_ = std::move(offset);
    placer.height_ = height;
    return placer;
}

FoliagePlacer FoliagePlacer::bush(IntProvider radius, IntProvider offset, const std::int32_t height) {
    FoliagePlacer placer = blob(std::move(radius), std::move(offset), height);
    placer.kind_ = Kind::Bush;
    return placer;
}

FoliagePlacer FoliagePlacer::megaJungle(IntProvider radius, IntProvider offset, const std::int32_t height) {
    FoliagePlacer placer = blob(std::move(radius), std::move(offset), height);
    placer.kind_ = Kind::MegaJungle;
    return placer;
}

FoliagePlacer FoliagePlacer::fancy(IntProvider radius, IntProvider offset, const std::int32_t height) {
    FoliagePlacer placer = blob(std::move(radius), std::move(offset), height);
    placer.kind_ = Kind::Fancy;
    return placer;
}

FoliagePlacer FoliagePlacer::acacia(IntProvider radius, IntProvider offset) {
    FoliagePlacer placer = blob(std::move(radius), std::move(offset), 0);
    placer.kind_ = Kind::Acacia;
    return placer;
}

FoliagePlacer FoliagePlacer::spruce(IntProvider radius, IntProvider offset, IntProvider trunkHeight) {
    FoliagePlacer placer = blob(std::move(radius), std::move(offset), 0);
    placer.kind_ = Kind::Spruce;
    placer.trunkHeight_ = std::move(trunkHeight);
    return placer;
}

FoliagePlacer FoliagePlacer::pine(IntProvider radius, IntProvider offset, IntProvider height) {
    FoliagePlacer placer = blob(std::move(radius), std::move(offset), 0);
    placer.kind_ = Kind::Pine;
    placer.heightProvider_ = std::move(height);
    return placer;
}

FoliagePlacer FoliagePlacer::darkOak(IntProvider radius, IntProvider offset) {
    FoliagePlacer placer = blob(std::move(radius), std::move(offset), 0);
    placer.kind_ = Kind::DarkOak;
    return placer;
}

FoliagePlacer FoliagePlacer::megaPine(IntProvider radius, IntProvider offset, IntProvider crownHeight) {
    FoliagePlacer placer = blob(std::move(radius), std::move(offset), 0);
    placer.kind_ = Kind::MegaPine;
    placer.heightProvider_ = std::move(crownHeight);
    return placer;
}

FoliagePlacer FoliagePlacer::randomSpread(IntProvider radius, IntProvider offset, IntProvider foliageHeight,
                                          const std::int32_t leafPlacementAttempts) {
    FoliagePlacer placer = blob(std::move(radius), std::move(offset), 0);
    placer.kind_ = Kind::RandomSpread;
    placer.heightProvider_ = std::move(foliageHeight);
    placer.leafPlacementAttempts_ = leafPlacementAttempts;
    return placer;
}

FoliagePlacer FoliagePlacer::weightedRandomSpread(IntProvider radius, IntProvider offset, IntProvider foliageHeight,
                                                  const std::int32_t leafPlacementAttempts,
                                                  std::vector<std::pair<std::int32_t, std::int32_t>> states) {
    FoliagePlacer placer =
        randomSpread(std::move(radius), std::move(offset), std::move(foliageHeight), leafPlacementAttempts);
    placer.kind_ = Kind::WeightedRandomSpread;
    for (const auto& [stateId, weight] : states) {
        placer.totalWeight_ += weight;
    }
    placer.weightedStates_ =
        std::make_shared<const std::vector<std::pair<std::int32_t, std::int32_t>>>(std::move(states));
    return placer;
}

FoliagePlacer FoliagePlacer::cherry(IntProvider radius, IntProvider offset, IntProvider height,
                                    const float wideBottomLayerHoleChance, const float cornerHoleChance,
                                    const float hangingLeavesChance, const float hangingLeavesExtensionChance) {
    FoliagePlacer placer = blob(std::move(radius), std::move(offset), 0);
    placer.kind_ = Kind::Cherry;
    placer.heightProvider_ = std::move(height);
    placer.wideBottomLayerHoleChance_ = wideBottomLayerHoleChance;
    placer.cornerHoleChance_ = cornerHoleChance;
    placer.hangingLeavesChance_ = hangingLeavesChance;
    placer.hangingLeavesExtensionChance_ = hangingLeavesExtensionChance;
    return placer;
}

std::int32_t FoliagePlacer::foliageRadius(random::RandomSource& random, const std::int32_t trunkHeight) const {
    const std::int32_t base = radius_.sample(random);
    if (kind_ == Kind::Pine) {
        return base + random.nextInt(std::max(trunkHeight + 1, 1));
    }
    return base;
}

std::int32_t FoliagePlacer::foliageHeight(random::RandomSource& random, const std::int32_t treeHeight) const {
    switch (kind_) {
    case Kind::Blob:
    case Kind::Bush:
    case Kind::MegaJungle:
    case Kind::Fancy:
        return height_;
    case Kind::Acacia:
        return 0;
    case Kind::DarkOak:
        return 4;
    case Kind::Spruce:
        return std::max(4, treeHeight - trunkHeight_.sample(random));
    case Kind::Pine:
    case Kind::MegaPine:
    case Kind::RandomSpread:
    case Kind::WeightedRandomSpread:
    case Kind::Cherry:
        return heightProvider_.sample(random);
    }
    return 0;
}

std::int32_t FoliagePlacer::offsetOf(random::RandomSource& random) const { return offset_.sample(random); }

bool FoliagePlacer::shouldSkipLocation(random::RandomSource& random, const std::int32_t dx, const std::int32_t y,
                                       const std::int32_t dz, const std::int32_t currentRadius,
                                       const bool doubleTrunk) const {
    switch (kind_) {
    case Kind::Blob:
        return dx == currentRadius && dz == currentRadius && (random.nextInt(2) == 0 || y == 0);
    case Kind::Bush:
        return dx == currentRadius && dz == currentRadius && random.nextInt(2) == 0;
    case Kind::MegaJungle:
    case Kind::MegaPine:
        return dx + dz >= 7 || dx * dx + dz * dz > currentRadius * currentRadius;
    case Kind::Fancy: {
        const float fx = static_cast<float>(dx) + 0.5F;
        const float fz = static_cast<float>(dz) + 0.5F;
        return fx * fx + fz * fz > static_cast<float>(currentRadius * currentRadius);
    }
    case Kind::Acacia:
        if (y == 0) {
            return (dx > 1 || dz > 1) && dx != 0 && dz != 0;
        }
        return dx == currentRadius && dz == currentRadius && currentRadius > 0;
    case Kind::Spruce:
    case Kind::Pine:
        return dx == currentRadius && dz == currentRadius && currentRadius > 0;
    case Kind::DarkOak:
        if (y == -1 && !doubleTrunk) {
            return dx == currentRadius && dz == currentRadius;
        }
        return y == 1 && dx + dz > currentRadius * 2 - 2;
    case Kind::RandomSpread:
    case Kind::WeightedRandomSpread:
        return false;
    case Kind::Cherry: {
        if (y == -1 && (dx == currentRadius || dz == currentRadius) &&
            random.nextFloat() < wideBottomLayerHoleChance_) {
            return true;
        }
        const bool corner = dx == currentRadius && dz == currentRadius;
        if (currentRadius > 2) {
            return corner || (dx + dz > currentRadius * 2 - 2 && random.nextFloat() < cornerHoleChance_);
        }
        return corner && random.nextFloat() < cornerHoleChance_;
    }
    }
    return false;
}

bool FoliagePlacer::shouldSkipLocationSigned(random::RandomSource& random, const std::int32_t dx, const std::int32_t y,
                                             const std::int32_t dz, const std::int32_t currentRadius,
                                             const bool doubleTrunk) const {
    if (kind_ == Kind::DarkOak && y == 0 && doubleTrunk && (dx == -currentRadius || dx >= currentRadius) &&
        (dz == -currentRadius || dz >= currentRadius)) {
        return true;
    }
    const std::int32_t minDx = doubleTrunk ? std::min(std::abs(dx), std::abs(dx - 1)) : std::abs(dx);
    const std::int32_t minDz = doubleTrunk ? std::min(std::abs(dz), std::abs(dz - 1)) : std::abs(dz);
    return shouldSkipLocation(random, minDx, y, minDz, currentRadius, doubleTrunk);
}

void FoliagePlacer::placeLeavesRow(Chunk& chunk, random::RandomSource& random, const std::int32_t foliageStateId,
                                   const std::int32_t originX, const std::int32_t originY, const std::int32_t originZ,
                                   const std::int32_t currentRadius, const std::int32_t y, const bool doubleTrunk,
                                   FoliagePositions* const foliagePositions) const {
    const std::int32_t extra = doubleTrunk ? 1 : 0;
    for (std::int32_t dx = -currentRadius; dx <= currentRadius + extra; ++dx) {
        for (std::int32_t dz = -currentRadius; dz <= currentRadius + extra; ++dz) {
            if (shouldSkipLocationSigned(random, dx, y, dz, currentRadius, doubleTrunk)) {
                continue;
            }
            const std::int32_t x = originX + dx;
            const std::int32_t leafY = originY + y;
            const std::int32_t z = originZ + dz;
            if (tryPlaceLeaf(chunk, foliageStateId, x, leafY, z) && foliagePositions != nullptr) {
                foliagePositions->emplace(x, leafY, z);
            }
        }
    }
}

bool FoliagePlacer::tryPlaceExtension(Chunk& chunk, random::RandomSource& random, const std::int32_t foliageStateId,
                                      const float chance, const std::int32_t logX, const std::int32_t logY,
                                      const std::int32_t logZ, const std::int32_t x, const std::int32_t y,
                                      const std::int32_t z, FoliagePositions& foliagePositions) const {
    if (std::abs(x - logX) + std::abs(y - logY) + std::abs(z - logZ) >= 7) {
        return false;
    }
    if (random.nextFloat() > chance) {
        return false;
    }
    if (!tryPlaceLeaf(chunk, foliageStateId, x, y, z)) {
        return false;
    }
    foliagePositions.emplace(x, y, z);
    return true;
}

void FoliagePlacer::placeLeavesRowWithHangingLeavesBelow(Chunk& chunk, random::RandomSource& random,
                                                         const std::int32_t foliageStateId, const std::int32_t originX,
                                                         const std::int32_t originY, const std::int32_t originZ,
                                                         const std::int32_t currentRadius, const std::int32_t y,
                                                         const bool doubleTrunk, const float hangingLeavesChance,
                                                         const float hangingLeavesExtensionChance,
                                                         FoliagePositions& foliagePositions) const {
    placeLeavesRow(chunk, random, foliageStateId, originX, originY, originZ, currentRadius, y, doubleTrunk,
                   &foliagePositions);
    const std::int32_t offset = doubleTrunk ? 1 : 0;
    const std::int32_t logY = originY - 1;
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::size_t edge = 0; edge < kAlongEdges.size(); ++edge) {
        const HorizontalDirection alongEdge = kAlongEdges[edge];
        const HorizontalDirection toEdge = kToEdges[edge];
        const std::int32_t offsetToEdge = kToEdgePositive[edge] ? currentRadius + offset : currentRadius;
        std::int32_t posX = originX + toEdge.dx * offsetToEdge - alongEdge.dx * currentRadius;
        const std::int32_t posY = originY + y - 1;
        std::int32_t posZ = originZ + toEdge.dz * offsetToEdge - alongEdge.dz * currentRadius;
        for (std::int32_t offsetAlongEdge = -currentRadius; offsetAlongEdge < currentRadius + offset;
             ++offsetAlongEdge) {
            const bool leavesAbove = posY + 1 >= bounds.minY() && posY + 1 <= bounds.maxY() &&
                                     foliagePositions.contains(std::tuple{posX, posY + 1, posZ});
            if (leavesAbove && tryPlaceExtension(chunk, random, foliageStateId, hangingLeavesChance, originX, logY,
                                                 originZ, posX, posY, posZ, foliagePositions)) {
                tryPlaceExtension(chunk, random, foliageStateId, hangingLeavesExtensionChance, originX, logY, originZ,
                                  posX, posY - 1, posZ, foliagePositions);
            }
            posX += alongEdge.dx;
            posZ += alongEdge.dz;
        }
    }
}

void FoliagePlacer::createFoliage(Chunk& chunk, random::RandomSource& random, const std::int32_t foliageStateId,
                                  const FoliageAttachment& attachment, const std::int32_t foliageHeight,
                                  const std::int32_t leafRadius, const std::int32_t offset,
                                  FoliagePositions& foliagePositions) const {
    switch (kind_) {
    case Kind::Blob:
        for (std::int32_t yo = offset; yo >= offset - foliageHeight; --yo) {
            const std::int32_t currentRadius = std::max(leafRadius + attachment.radiusOffset - 1 - yo / 2, 0);
            placeLeavesRow(chunk, random, foliageStateId, attachment.x, attachment.y, attachment.z, currentRadius, yo,
                           attachment.doubleTrunk);
        }
        return;
    case Kind::Bush:
        for (std::int32_t y = offset; y >= offset - foliageHeight; --y) {
            placeLeavesRow(chunk, random, foliageStateId, attachment.x, attachment.y, attachment.z,
                           leafRadius + attachment.radiusOffset - 1 - y, y, attachment.doubleTrunk);
        }
        return;
    case Kind::MegaJungle: {
        const std::int32_t leafHeight = attachment.doubleTrunk ? foliageHeight : 1 + random.nextInt(2);
        for (std::int32_t y = offset; y >= offset - leafHeight; --y) {
            placeLeavesRow(chunk, random, foliageStateId, attachment.x, attachment.y, attachment.z,
                           leafRadius + attachment.radiusOffset + 1 - y, y, attachment.doubleTrunk);
        }
        return;
    }
    case Kind::Fancy:
        for (std::int32_t y = offset; y >= offset - foliageHeight; --y) {
            const std::int32_t currentRadius = leafRadius + (y != offset && y != offset - foliageHeight ? 1 : 0);
            placeLeavesRow(chunk, random, foliageStateId, attachment.x, attachment.y, attachment.z, currentRadius, y,
                           attachment.doubleTrunk);
        }
        return;
    case Kind::Acacia: {
        const std::int32_t fx = attachment.x;
        const std::int32_t fy = attachment.y + offset;
        const std::int32_t fz = attachment.z;
        placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + attachment.radiusOffset,
                       -1 - foliageHeight, attachment.doubleTrunk);
        placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius - 1, -foliageHeight,
                       attachment.doubleTrunk);
        placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + attachment.radiusOffset - 1, 0,
                       attachment.doubleTrunk);
        return;
    }
    case Kind::Spruce: {
        std::int32_t currentRadius = random.nextInt(2);
        std::int32_t maxRadius = 1;
        std::int32_t minRadius = 0;
        for (std::int32_t yo = offset; yo >= -foliageHeight; --yo) {
            placeLeavesRow(chunk, random, foliageStateId, attachment.x, attachment.y, attachment.z, currentRadius, yo,
                           attachment.doubleTrunk);
            if (currentRadius >= maxRadius) {
                currentRadius = minRadius;
                minRadius = 1;
                maxRadius = std::min(maxRadius + 1, leafRadius + attachment.radiusOffset);
            } else {
                currentRadius++;
            }
        }
        return;
    }
    case Kind::Pine: {
        std::int32_t currentRadius = 0;
        for (std::int32_t yo = offset; yo >= offset - foliageHeight; --yo) {
            placeLeavesRow(chunk, random, foliageStateId, attachment.x, attachment.y, attachment.z, currentRadius, yo,
                           attachment.doubleTrunk);
            if (currentRadius >= 1 && yo == offset - foliageHeight + 1) {
                currentRadius--;
            } else if (currentRadius < leafRadius + attachment.radiusOffset) {
                currentRadius++;
            }
        }
        return;
    }
    case Kind::DarkOak: {
        const std::int32_t fx = attachment.x;
        const std::int32_t fy = attachment.y + offset;
        const std::int32_t fz = attachment.z;
        const bool doubleTrunk = attachment.doubleTrunk;
        if (doubleTrunk) {
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + 2, -1, doubleTrunk);
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + 3, 0, doubleTrunk);
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + 2, 1, doubleTrunk);
            if (random.nextBoolean()) {
                placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius, 2, doubleTrunk);
            }
        } else {
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + 2, -1, doubleTrunk);
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + 1, 0, doubleTrunk);
        }
        return;
    }
    case Kind::MegaPine: {
        std::int32_t prevRadius = 0;
        const std::int32_t topY = attachment.y + offset;
        const std::int32_t bottomY = attachment.y - foliageHeight + offset;
        for (std::int32_t yy = bottomY; yy <= topY; ++yy) {
            const std::int32_t yo = attachment.y - yy;
            const std::int32_t smoothRadius = leafRadius + attachment.radiusOffset +
                                              static_cast<std::int32_t>(std::floor(
                                                  static_cast<float>(yo) / static_cast<float>(foliageHeight) * 3.5F));
            const std::int32_t jaggedRadius =
                yo > 0 && smoothRadius == prevRadius && (yy & 1) == 0 ? smoothRadius + 1 : smoothRadius;
            placeLeavesRow(chunk, random, foliageStateId, attachment.x, yy, attachment.z, jaggedRadius, 0,
                           attachment.doubleTrunk);
            prevRadius = smoothRadius;
        }
        return;
    }
    case Kind::RandomSpread:
        for (std::int32_t attempt = 0; attempt < leafPlacementAttempts_; ++attempt) {
            const std::int32_t dx = random.nextInt(leafRadius) - random.nextInt(leafRadius);
            const std::int32_t dy = random.nextInt(foliageHeight) - random.nextInt(foliageHeight);
            const std::int32_t dz = random.nextInt(leafRadius) - random.nextInt(leafRadius);
            tryPlaceLeaf(chunk, foliageStateId, attachment.x + dx, attachment.y + dy, attachment.z + dz);
        }
        return;
    case Kind::WeightedRandomSpread:
        for (std::int32_t attempt = 0; attempt < leafPlacementAttempts_; ++attempt) {
            const std::int32_t x = attachment.x + random.nextInt(leafRadius) - random.nextInt(leafRadius);
            const std::int32_t y = attachment.y + random.nextInt(foliageHeight) - random.nextInt(foliageHeight);
            const std::int32_t z = attachment.z + random.nextInt(leafRadius) - random.nextInt(leafRadius);
            if (!canPlaceLeaf(chunk, x, y, z)) {
                continue;
            }
            std::int32_t roll = random.nextInt(totalWeight_);
            std::int32_t chosen = foliageStateId;
            for (const auto& [stateId, weight] : *weightedStates_) {
                roll -= weight;
                if (roll < 0) {
                    chosen = stateId;
                    break;
                }
            }
            tryPlaceLeaf(chunk, chosen, x, y, z);
        }
        return;
    case Kind::Cherry: {
        const bool doubleTrunk = attachment.doubleTrunk;
        const std::int32_t fx = attachment.x;
        const std::int32_t fy = attachment.y + offset;
        const std::int32_t fz = attachment.z;
        const std::int32_t currentRadius = leafRadius + attachment.radiusOffset - 1;
        placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, currentRadius - 2, foliageHeight - 3, doubleTrunk,
                       &foliagePositions);
        placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, currentRadius - 1, foliageHeight - 4, doubleTrunk,
                       &foliagePositions);
        for (std::int32_t y = foliageHeight - 5; y >= 0; --y) {
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, currentRadius, y, doubleTrunk, &foliagePositions);
        }
        placeLeavesRowWithHangingLeavesBelow(chunk, random, foliageStateId, fx, fy, fz, currentRadius, -1, doubleTrunk,
                                             hangingLeavesChance_, hangingLeavesExtensionChance_, foliagePositions);
        placeLeavesRowWithHangingLeavesBelow(chunk, random, foliageStateId, fx, fy, fz, currentRadius - 1, -2,
                                             doubleTrunk, hangingLeavesChance_, hangingLeavesExtensionChance_,
                                             foliagePositions);
        return;
    }
    }
}

} // namespace basalt::server::world::terrain::decoration::tree
