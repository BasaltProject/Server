#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/terrain/PillagerOutpostStructureGenerator.hpp>
#include <basalt/server/world/terrain/StructureBeardifier.hpp>

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

using structure::PlacedStructurePiece;
using structure::StructureBoundingBox;
using structure::StructurePos;

/// parity: `start_height: {absolute: 0}`, which draws nothing from the random.
constexpr std::int32_t kStartHeight = 0;
constexpr std::int32_t kBeardRadius = 12;
constexpr std::size_t kCacheCapacity = 32;
constexpr std::int32_t kCellWidth = 4;
constexpr std::int32_t kCellHeight = 8;
constexpr std::int32_t kSeaLevel = 63;

[[nodiscard]] StructureBoundingBox chunkClip(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t margin) {
    return StructureBoundingBox{chunkX * 16 - margin,      kOverworldHeight.minY(), chunkZ * 16 - margin,
                                chunkX * 16 + 15 + margin, kOverworldHeight.maxY(), chunkZ * 16 + 15 + margin};
}

[[nodiscard]] std::int32_t floorDiv(std::int32_t value, std::int32_t divisor) {
    const std::int32_t quotient = value / divisor;
    return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
}

} // namespace

PillagerOutpostStructureGenerator::PillagerOutpostStructureGenerator(OverworldNoiseRouter router,
                                                                     structure::StructurePlacementGrid grid,
                                                                     BiomeAt biomeAt,
                                                                     std::set<std::string, std::less<>> outpostBiomes,
                                                                     PoolLookup poolLookup,
                                                                     structure::PoolTemplateLoader templateLoader)
    : router_(std::move(router)), grid_(std::move(grid)), biomeAt_(std::move(biomeAt)),
      outpostBiomes_(std::move(outpostBiomes)), poolLookup_(std::move(poolLookup)),
      templateLoader_(std::move(templateLoader)) {}

std::pair<std::size_t, std::int64_t> PillagerOutpostStructureGenerator::assemblyStats() const {
    const std::scoped_lock lock{mutex_};
    std::int64_t pieces = 0;
    for (const auto& [key, start] : cache_) {
        pieces += static_cast<std::int64_t>(start.size());
    }
    return {cache_.size(), pieces};
}

std::int32_t PillagerOutpostStructureGenerator::surfaceHeightAt(std::int32_t x, std::int32_t z,
                                                                Samplers& samplers) const {
    const std::int32_t chunkX = x >> 4;
    const std::int32_t chunkZ = z >> 4;
    const std::int64_t key =
        (static_cast<std::int64_t>(chunkX) & 0xFFFFFFFFLL) | (static_cast<std::int64_t>(chunkZ) << 32);
    auto found = samplers.find(key);
    if (found == samplers.end()) {
        found = samplers
                    .emplace(std::piecewise_construct, std::forward_as_tuple(key),
                             std::forward_as_tuple(router_.finalDensity, chunkX, chunkZ, kOverworldHeight.minY(),
                                                   kOverworldHeight.height(), kCellWidth, kCellHeight))
                    .first;
    }
    const std::int32_t localX = x & 15;
    const std::int32_t localZ = z & 15;
    std::int32_t y = kOverworldHeight.maxY();
    while (y >= kOverworldHeight.minY() && found->second.sample(localX, y, localZ) <= 0.0) {
        --y;
    }
    return std::max(y + 1, kSeaLevel);
}

bool PillagerOutpostStructureGenerator::inOutpostBiome(std::int32_t candidateChunkX, std::int32_t candidateChunkZ,
                                                       Samplers& samplers) const {
    const std::int32_t centreX = candidateChunkX * 16 + 8;
    const std::int32_t centreZ = candidateChunkZ * 16 + 8;
    // The tag keeps its namespace and so does the biome name, so neither side is stripped.
    return outpostBiomes_.contains(biomeAt_(centreX, surfaceHeightAt(centreX, centreZ, samplers), centreZ));
}

const std::vector<PlacedStructurePiece>& PillagerOutpostStructureGenerator::assembledStart(std::int32_t candidateChunkX,
                                                                                           std::int32_t candidateChunkZ,
                                                                                           std::int64_t seed) {
    const CandidateKey key{seed, candidateChunkX, candidateChunkZ};
    {
        const std::scoped_lock lock{mutex_};
        const auto found = cache_.find(key);
        if (found != cache_.end()) {
            return found->second;
        }
    }

    Samplers samplers;
    std::vector<PlacedStructurePiece> assembled;
    if (grid_.isStructureChunk(seed, candidateChunkX, candidateChunkZ) &&
        inOutpostBiome(candidateChunkX, candidateChunkZ, samplers)) {
        random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
        random::setLargeFeatureSeed(random, seed, candidateChunkX, candidateChunkZ);
        const structure::JigsawWorld world{
            .poolLookup = poolLookup_,
            .templateLoader = templateLoader_,
            .heightAt = [this, &samplers](std::int32_t x, std::int32_t z) { return surfaceHeightAt(x, z, samplers); }};
        assembled = structure::assembleJigsaw(
            world,
            structure::JigsawRequest{.startPool = std::string{kStartPool},
                                     .startPosition =
                                         StructurePos{candidateChunkX * 16, kStartHeight, candidateChunkZ * 16},
                                     .maxDepth = kMaxDepth,
                                     .projectStartToHeightmap = true,
                                     .maxDistanceFromCenter = kMaxDistanceFromCenter,
                                     .useExpansionHack = true},
            random);
    }

    const std::scoped_lock lock{mutex_};
    const auto found = cache_.find(key);
    if (found != cache_.end()) {
        return found->second;
    }
    if (cache_.size() >= kCacheCapacity) {
        cache_.clear();
    }
    return cache_.emplace(key, std::move(assembled)).first->second;
}

std::vector<PillagerOutpostStructureGenerator::Start>
PillagerOutpostStructureGenerator::nearbyStarts(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed,
                                                const StructureBoundingBox& clip) {
    const std::int32_t cellX = floorDiv(chunkX, grid_.spacing());
    const std::int32_t cellZ = floorDiv(chunkZ, grid_.spacing());
    const std::int32_t reachChunks = (kMaxDistanceFromCenter + kBeardRadius) / 16 + 1;
    std::vector<Start> starts;
    for (std::int32_t cellOffsetX = -1; cellOffsetX <= 1; ++cellOffsetX) {
        for (std::int32_t cellOffsetZ = -1; cellOffsetZ <= 1; ++cellOffsetZ) {
            const auto [candidateX, candidateZ] = grid_.candidateChunk(seed, (cellX + cellOffsetX) * grid_.spacing(),
                                                                       (cellZ + cellOffsetZ) * grid_.spacing());
            if (std::abs(candidateX - chunkX) > reachChunks || std::abs(candidateZ - chunkZ) > reachChunks) {
                continue;
            }
            const std::vector<PlacedStructurePiece>& pieces = assembledStart(candidateX, candidateZ, seed);
            if (pieces.empty()) {
                continue;
            }
            const bool touches = std::any_of(pieces.begin(), pieces.end(), [&clip](const PlacedStructurePiece& piece) {
                return piece.boundingBox.intersects(clip);
            });
            if (touches) {
                starts.emplace_back(CandidateKey{seed, candidateX, candidateZ}, &pieces);
            }
        }
    }
    return starts;
}

void PillagerOutpostStructureGenerator::recordStart(Chunk& chunk, const StructureBoundingBox& clip, CandidateKey origin,
                                                    const std::vector<PlacedStructurePiece>& pieces) {
    std::vector<StructureBoundingBox> here;
    std::vector<StructureBoundingBox> all;
    all.reserve(pieces.size());
    for (const PlacedStructurePiece& piece : pieces) {
        all.push_back(piece.boundingBox);
        if (piece.boundingBox.intersects(clip)) {
            here.push_back(piece.boundingBox);
        }
    }
    if (here.empty()) {
        return;
    }
    const std::optional<structure::StructureStartRecord> whole =
        structure::StructureStartRecord::of(std::string{kName}, 0, 0, std::move(all));
    if (!whole.has_value()) {
        return;
    }
    chunk.structures().add(structure::StructureStartRecord{.structure = std::string{kName},
                                                           .chunkX = origin.chunkX,
                                                           .chunkZ = origin.chunkZ,
                                                           .fullBounds = whole->fullBounds,
                                                           .pieces = std::move(here)});
}

void PillagerOutpostStructureGenerator::generateInto(Chunk& chunk, std::int32_t chunkX, std::int32_t chunkZ,
                                                     std::int64_t seed) {
    const StructureBoundingBox clip = chunkClip(chunkX, chunkZ, 0);
    const std::vector<Start> starts = nearbyStarts(chunkX, chunkZ, seed, clip);
    if (starts.empty()) {
        return;
    }
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed = random::setDecorationSeed(random, seed, chunkX * 16, chunkZ * 16);
    random::setFeatureSeed(random, decorationSeed, kStructureIndex, kSurfaceStructuresStep);
    for (const auto& [origin, pieces] : starts) {
        recordStart(chunk, clip, origin, *pieces);
        for (const PlacedStructurePiece& piece : *pieces) {
            if (!piece.boundingBox.intersects(clip)) {
                continue;
            }
            piece.element.place(templateLoader_, chunk, piece.position, piece.rotation, clip, random);
        }
    }
}

densityfunction::Function PillagerOutpostStructureGenerator::beardified(densityfunction::Function base,
                                                                        std::int32_t chunkX, std::int32_t chunkZ,
                                                                        std::int64_t seed) {
    const StructureBoundingBox nearClip = chunkClip(chunkX, chunkZ, kBeardRadius);
    const std::int32_t chunkMinX = chunkX * 16;
    const std::int32_t chunkMinZ = chunkZ * 16;
    std::vector<BeardifierRigid> rigids;
    std::vector<BeardifierJunction> junctions;
    for (const auto& [origin, pieces] : nearbyStarts(chunkX, chunkZ, seed, nearClip)) {
        for (const PlacedStructurePiece& piece : *pieces) {
            if (!piece.boundingBox.intersects(nearClip)) {
                continue;
            }
            if (piece.element.projection() == structure::StructureProjection::Rigid) {
                rigids.push_back(BeardifierRigid{.box = piece.boundingBox,
                                                 .groundLevelDelta = piece.groundLevelDelta,
                                                 .adjustment = TerrainAdjustment::BeardThin});
            }
            for (const structure::PlacedJigsawJunction& junction : piece.junctions) {
                if (junction.sourceX > chunkMinX - kBeardRadius && junction.sourceX < chunkMinX + 15 + kBeardRadius &&
                    junction.sourceZ > chunkMinZ - kBeardRadius && junction.sourceZ < chunkMinZ + 15 + kBeardRadius) {
                    junctions.push_back(BeardifierJunction{
                        .x = junction.sourceX, .groundY = junction.sourceGroundY, .z = junction.sourceZ});
                }
            }
        }
    }
    if (rigids.empty() && junctions.empty()) {
        return base;
    }
    return densityfunction::functions::add(std::move(base),
                                           structureBeardifier(std::move(rigids), std::move(junctions)));
}

std::optional<std::pair<std::int32_t, std::int32_t>> PillagerOutpostStructureGenerator::locate(std::int64_t seed,
                                                                                               std::int32_t fromChunkX,
                                                                                               std::int32_t fromChunkZ,
                                                                                               std::int32_t maxCells) {
    const std::int32_t cellX = floorDiv(fromChunkX, grid_.spacing());
    const std::int32_t cellZ = floorDiv(fromChunkZ, grid_.spacing());
    for (std::int32_t ring = 0; ring <= maxCells; ++ring) {
        for (std::int32_t dx = -ring; dx <= ring; ++dx) {
            for (std::int32_t dz = -ring; dz <= ring; ++dz) {
                if (ring != 0 && std::abs(dx) != ring && std::abs(dz) != ring) {
                    continue;
                }
                const auto [candidateX, candidateZ] =
                    grid_.candidateChunk(seed, (cellX + dx) * grid_.spacing(), (cellZ + dz) * grid_.spacing());
                if (assembledStart(candidateX, candidateZ, seed).empty()) {
                    continue;
                }
                return std::pair{candidateX, candidateZ};
            }
        }
    }
    return std::nullopt;
}

} // namespace basalt::server::world::terrain
