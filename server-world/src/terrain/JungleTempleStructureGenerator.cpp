#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/terrain/JungleTempleStructureGenerator.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

using structure::StructureBoundingBox;
using structure::StructureDirection;

constexpr std::int32_t kSurfaceStructuresStep = 4;
/// parity: the Overworld noise settings' `sea_level`.
constexpr std::int32_t kSeaLevel = 63;
constexpr std::int32_t kReachChunks = 2;

/// parity: `Direction.Plane.HORIZONTAL`, the array `getRandomDirection` draws from.
constexpr std::array<StructureDirection, 4> kHorizontal{StructureDirection::North, StructureDirection::East,
                                                        StructureDirection::South, StructureDirection::West};

[[nodiscard]] StructureBoundingBox chunkClip(std::int32_t chunkX, std::int32_t chunkZ) {
    return StructureBoundingBox{chunkX * 16,      kOverworldHeight.minY(), chunkZ * 16,
                                chunkX * 16 + 15, kOverworldHeight.maxY(), chunkZ * 16 + 15};
}

[[nodiscard]] std::int32_t floorDiv(std::int32_t value, std::int32_t divisor) {
    const std::int32_t quotient = value / divisor;
    return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
}

} // namespace

JungleTempleStructureGenerator::JungleTempleStructureGenerator(BiomeAt biomeAt,
                                                               std::set<std::string, std::less<>> templeBiomes,
                                                               SurfaceHeightAt surfaceHeightAt)
    : biomeAt_(std::move(biomeAt)), templeBiomes_(std::move(templeBiomes)),
      surfaceHeightAt_(std::move(surfaceHeightAt)) {}

const structure::StructurePlacementGrid& JungleTempleStructureGenerator::grid() {
    static const structure::StructurePlacementGrid built{32, 8, 14357619};
    return built;
}

std::optional<JungleTemplePiece> JungleTempleStructureGenerator::startPiece(std::int64_t seed, std::int32_t chunkX,
                                                                            std::int32_t chunkZ) const {
    if (!grid().isStructureChunk(seed, chunkX, chunkZ)) {
        return std::nullopt;
    }

    random::RandomSource startRandom = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(startRandom, seed, chunkX, chunkZ);

    const std::int32_t centreX = chunkX * 16 + 8;
    const std::int32_t centreZ = chunkZ * 16 + 8;
    const std::int32_t stubY = surfaceHeightAt_(centreX, centreZ) - 1;
    if (!templeBiomes_.contains(biomeAt_(centreX, stubY, centreZ))) {
        return std::nullopt;
    }
    // parity: `SinglePieceStructure.findGenerationPoint` refuses a start whose lowest corner, over
    // the piece's 12 by 15 footprint measured one past its far edges, is below sea level.
    const std::int32_t minX = chunkX * 16;
    const std::int32_t minZ = chunkZ * 16;
    const auto occupied = [this](std::int32_t x, std::int32_t z) { return surfaceHeightAt_(x, z) - 1; };
    if (std::min({occupied(minX, minZ), occupied(minX, minZ + 15), occupied(minX + 12, minZ),
                  occupied(minX + 12, minZ + 15)}) < kSeaLevel) {
        return std::nullopt;
    }

    const StructureDirection orientation =
        kHorizontal[static_cast<std::size_t>(startRandom.nextInt(static_cast<std::int32_t>(kHorizontal.size())))];
    JungleTemplePiece piece{chunkX * 16, chunkZ * 16, orientation};
    if (!piece.settleOnGround(surfaceHeightAt_)) {
        return std::nullopt;
    }
    return piece;
}

void JungleTempleStructureGenerator::placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX,
                                               std::int32_t chunkZ) const {
    std::optional<JungleTemplePiece> start = startPiece(seed, chunkX, chunkZ);
    if (!start.has_value()) {
        return;
    }
    JungleTemplePiece& piece = *start;
    const StructureBoundingBox clip = chunkClip(chunk.chunkX(), chunk.chunkZ());
    if (!piece.bounds().intersects(clip)) {
        return;
    }

    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kStructureIndex, kSurfaceStructuresStep);

    piece.place(chunk, clip, random);
    chunk.structures().add(structure::StructureStartRecord{.structure = std::string{kName},
                                                           .chunkX = chunkX,
                                                           .chunkZ = chunkZ,
                                                           .fullBounds = piece.bounds(),
                                                           .pieces = {piece.bounds()}});
}

void JungleTempleStructureGenerator::generateInto(Chunk& chunk, std::int64_t seed) {
    const std::int32_t spacing = grid().spacing();
    const std::int32_t cellX = floorDiv(chunk.chunkX(), spacing);
    const std::int32_t cellZ = floorDiv(chunk.chunkZ(), spacing);
    for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
        for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
            const auto [candidateX, candidateZ] =
                grid().candidateChunk(seed, (cellX + offsetX) * spacing, (cellZ + offsetZ) * spacing);
            if (std::abs(candidateX - chunk.chunkX()) > kReachChunks ||
                std::abs(candidateZ - chunk.chunkZ()) > kReachChunks) {
                continue;
            }
            placeFrom(chunk, seed, candidateX, candidateZ);
        }
    }
}

} // namespace basalt::server::world::terrain
