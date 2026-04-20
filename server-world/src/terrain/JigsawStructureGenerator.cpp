#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/terrain/JigsawStructureGenerator.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

using structure::PlacedStructurePiece;
using structure::StructureBoundingBox;
using structure::StructurePos;

constexpr std::int32_t kBeardRadius = 12;
constexpr std::int32_t kSeaLevel = 63;

[[nodiscard]] std::int32_t floorDiv(std::int32_t value, std::int32_t divisor) {
    const std::int32_t quotient = value / divisor;
    return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
}

} // namespace

JigsawStructureGenerator::JigsawStructureGenerator(JigsawStructureSettings settings, NoiseRouter router,
                                                   structure::StructurePlacementGrid grid, BiomeAt biomeAt,
                                                   std::set<std::string, std::less<>> biomes, PoolLookup poolLookup,
                                                   structure::PoolTemplateLoader templateLoader)
    : settings_(std::move(settings)), router_(std::move(router)), grid_(std::move(grid)), biomeAt_(std::move(biomeAt)),
      biomes_(std::move(biomes)), poolLookup_(std::move(poolLookup)), templateLoader_(std::move(templateLoader)) {}

std::pair<std::size_t, std::int64_t> JigsawStructureGenerator::assemblyStats() const {
    const std::scoped_lock lock{mutex_};
    std::int64_t pieces = 0;
    for (const auto& [key, start] : cache_) {
        pieces += start == nullptr ? 0 : static_cast<std::int64_t>(start->size());
    }
    return {cache_.size(), pieces};
}

StructureBoundingBox JigsawStructureGenerator::chunkClip(std::int32_t chunkX, std::int32_t chunkZ,
                                                         std::int32_t margin) const {
    return StructureBoundingBox{chunkX * 16 - margin,      kOverworldHeight.minY(), chunkZ * 16 - margin,
                                chunkX * 16 + 15 + margin, kOverworldHeight.maxY(), chunkZ * 16 + 15 + margin};
}

std::int32_t JigsawStructureGenerator::surfaceHeightAt(std::int32_t x, std::int32_t z, Samplers& samplers) const {
    const std::int32_t chunkX = x >> 4;
    const std::int32_t chunkZ = z >> 4;
    const std::int64_t key =
        (static_cast<std::int64_t>(chunkX) & 0xFFFFFFFFLL) | (static_cast<std::int64_t>(chunkZ) << 32);
    auto found = samplers.find(key);
    if (found == samplers.end()) {
        found = samplers
                    .emplace(std::piecewise_construct, std::forward_as_tuple(key),
                             std::forward_as_tuple(router_, chunkX, chunkZ))
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

std::vector<PlacedStructurePiece> JigsawStructureGenerator::buildStart(std::int32_t candidateChunkX,
                                                                       std::int32_t candidateChunkZ, std::int64_t seed,
                                                                       bool forPlacement) {
    Samplers samplers;
    std::vector<PlacedStructurePiece> assembled;
    if (forPlacement || grid_.isStructureChunk(seed, candidateChunkX, candidateChunkZ)) {
        random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
        random::setLargeFeatureSeed(random, seed, candidateChunkX, candidateChunkZ);
        // parity: a drawn start height is a uniform provider and costs a draw before the assembly's own.
        const std::int32_t startY = settings_.heightSource == JigsawHeightSource::Drawn
                                        ? settings_.startY + random.nextInt(settings_.startYMax - settings_.startY + 1)
                                        : settings_.startY;
        // parity: `PoolAliasLookup.create` forks a positional factory.
        std::map<std::string, std::string, std::less<>> resolvedAliases;
        if (!settings_.poolAliases.empty()) {
            random::RandomSource aliasRandom = random::RandomSource::legacy(seed).forkPositional().at(
                candidateChunkX * 16, startY, candidateChunkZ * 16);
            for (const JigsawStructureSettings::PoolAlias& binding : settings_.poolAliases) {
                if (binding.kind == JigsawStructureSettings::PoolAlias::Kind::Random) {
                    const std::size_t pick = static_cast<std::size_t>(
                        aliasRandom.nextInt(static_cast<std::int32_t>(binding.targets.size())));
                    resolvedAliases.insert_or_assign(binding.alias, binding.targets[pick]);
                } else {
                    const std::size_t pick =
                        static_cast<std::size_t>(aliasRandom.nextInt(static_cast<std::int32_t>(binding.groups.size())));
                    for (const auto& [alias, target] : binding.groups[pick]) {
                        resolvedAliases.insert_or_assign(alias, target);
                    }
                }
            }
        }
        structure::JigsawWorld world{.poolLookup = poolLookup_, .templateLoader = templateLoader_, .heightAt = {}};
        if (settings_.heightSource == JigsawHeightSource::Surface) {
            world.heightAt = [this, &samplers](std::int32_t x, std::int32_t z) {
                return surfaceHeightAt(x, z, samplers);
            };
        } else {
            world.heightAt = [startY](std::int32_t, std::int32_t) { return startY; };
        }
        assembled = structure::assembleJigsaw(
            world,
            structure::JigsawRequest{.startPool = settings_.startPool,
                                     .startPosition = StructurePos{candidateChunkX * 16, startY, candidateChunkZ * 16},
                                     .maxDepth = settings_.maxDepth,
                                     .projectStartToHeightmap = settings_.projectStartToHeightmap,
                                     .maxDistanceFromCenter = settings_.maxDistanceFromCenter,
                                     // parity: `MaxDistance(int)` gives one number to both axes.
                                     .maxVerticalDistanceFromCenter = settings_.maxDistanceFromCenter,
                                     .dimensionPaddingBottom = settings_.dimensionPadding,
                                     .dimensionPaddingTop = settings_.dimensionPadding,
                                     .worldMinY = kOverworldHeight.minY(),
                                     .worldMaxY = kOverworldHeight.maxY(),
                                     .useExpansionHack = settings_.useExpansionHack,
                                     .startJigsawName = settings_.startJigsawName,
                                     .poolAliases = std::move(resolvedAliases),
                                     .acceptStart =
                                         [this, forPlacement](const StructurePos& stub) {
                                             // parity: `/place structure` passes an always-true biome predicate.
                                             return forPlacement || biomes_.contains(biomeAt_(stub.x, stub.y, stub.z));
                                         }},
            random);
    }
    return assembled;
}

std::shared_ptr<const std::vector<PlacedStructurePiece>>
JigsawStructureGenerator::assembledStart(std::int32_t candidateChunkX, std::int32_t candidateChunkZ,
                                         std::int64_t seed) {
    const CandidateKey key{seed, candidateChunkX, candidateChunkZ};
    {
        const std::scoped_lock lock{mutex_};
        const auto found = cache_.find(key);
        if (found != cache_.end()) {
            return found->second;
        }
    }

    std::vector<PlacedStructurePiece> assembled = buildStart(candidateChunkX, candidateChunkZ, seed, false);

    const std::scoped_lock lock{mutex_};
    const auto found = cache_.find(key);
    if (found != cache_.end()) {
        return found->second;
    }
    if (cache_.size() >= settings_.cacheCapacity) {
        cache_.clear();
    }
    return cache_.emplace(key, std::make_shared<const std::vector<PlacedStructurePiece>>(std::move(assembled)))
        .first->second;
}

std::optional<std::vector<PlacedStructurePiece>>
JigsawStructureGenerator::assembleAt(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed) {
    std::vector<PlacedStructurePiece> assembled = buildStart(chunkX, chunkZ, seed, /*forPlacement=*/true);
    if (assembled.empty()) {
        return std::nullopt;
    }
    return assembled;
}

std::optional<structure::StructureBoundingBox>
JigsawStructureGenerator::placeBoundsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) {
    const std::optional<std::vector<PlacedStructurePiece>> assembled = assembleAt(chunkX, chunkZ, seed);
    if (!assembled.has_value()) {
        return std::nullopt;
    }
    std::optional<StructureBoundingBox> whole;
    for (const PlacedStructurePiece& piece : *assembled) {
        whole = whole.has_value() ? whole->encapsulating(piece.boundingBox) : piece.boundingBox;
    }
    return whole;
}

void JigsawStructureGenerator::placeAt(Chunk& chunk, std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed) {
    const std::optional<std::vector<PlacedStructurePiece>> assembled = assembleAt(chunkX, chunkZ, seed);
    if (!assembled.has_value()) {
        return;
    }
    const StructureBoundingBox clip = chunkClip(chunk.chunkX(), chunk.chunkZ(), 0);
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, settings_.structureIndex, settings_.step);
    const std::optional<std::int64_t> worldSeed =
        settings_.passWorldSeed ? std::optional<std::int64_t>{seed} : std::nullopt;
    for (const PlacedStructurePiece& piece : *assembled) {
        if (!piece.boundingBox.intersects(clip)) {
            continue;
        }
        piece.element.place(templateLoader_, chunk, piece.position, piece.rotation, clip, random, worldSeed);
    }
    // parity: `PlaceCommand.placeStructure` writes the pieces but registers no start.
}

std::vector<JigsawStructureGenerator::Start> JigsawStructureGenerator::nearbyStarts(std::int32_t chunkX,
                                                                                    std::int32_t chunkZ,
                                                                                    std::int64_t seed,
                                                                                    const StructureBoundingBox& clip) {
    const std::int32_t cellX = floorDiv(chunkX, grid_.spacing());
    const std::int32_t cellZ = floorDiv(chunkZ, grid_.spacing());
    const std::int32_t reachChunks = (settings_.maxDistanceFromCenter + kBeardRadius) / 16 + 1;
    std::vector<Start> starts;
    for (std::int32_t cellOffsetX = -1; cellOffsetX <= 1; ++cellOffsetX) {
        for (std::int32_t cellOffsetZ = -1; cellOffsetZ <= 1; ++cellOffsetZ) {
            const auto [candidateX, candidateZ] = grid_.candidateChunk(seed, (cellX + cellOffsetX) * grid_.spacing(),
                                                                       (cellZ + cellOffsetZ) * grid_.spacing());
            if (std::abs(candidateX - chunkX) > reachChunks || std::abs(candidateZ - chunkZ) > reachChunks) {
                continue;
            }
            const std::shared_ptr<const std::vector<PlacedStructurePiece>> assembled =
                assembledStart(candidateX, candidateZ, seed);
            if (assembled == nullptr || assembled->empty()) {
                continue;
            }
            const std::vector<PlacedStructurePiece>& pieces = *assembled;
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

void JigsawStructureGenerator::recordStart(Chunk& chunk, const StructureBoundingBox& clip, CandidateKey origin,
                                           const std::vector<PlacedStructurePiece>& pieces) const {
    if (settings_.startRecords == JigsawStartRecords::None || pieces.empty()) {
        return;
    }
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
        structure::StructureStartRecord::of(settings_.name, 0, 0, std::move(all));
    if (!whole.has_value()) {
        return;
    }
    const bool fromPiece = settings_.startRecords == JigsawStartRecords::FromFirstPiece;
    chunk.structures().add(
        structure::StructureStartRecord{.structure = settings_.name,
                                        .chunkX = fromPiece ? pieces.front().position.x >> 4 : origin.chunkX,
                                        .chunkZ = fromPiece ? pieces.front().position.z >> 4 : origin.chunkZ,
                                        .fullBounds = whole->fullBounds,
                                        .pieces = std::move(here)});
}

void JigsawStructureGenerator::generateInto(Chunk& chunk, std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed) {
    const StructureBoundingBox clip = chunkClip(chunkX, chunkZ, 0);
    const std::vector<Start> starts = nearbyStarts(chunkX, chunkZ, seed, clip);
    if (starts.empty()) {
        return;
    }
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed = random::setDecorationSeed(random, seed, chunkX * 16, chunkZ * 16);
    random::setFeatureSeed(random, decorationSeed, settings_.structureIndex, settings_.step);
    const std::optional<std::int64_t> worldSeed =
        settings_.passWorldSeed ? std::optional<std::int64_t>{seed} : std::nullopt;
    for (const auto& [origin, pieces] : starts) {
        recordStart(chunk, clip, origin, *pieces);
        for (const PlacedStructurePiece& piece : *pieces) {
            if (!piece.boundingBox.intersects(clip)) {
                continue;
            }
            piece.element.place(templateLoader_, chunk, piece.position, piece.rotation, clip, random, worldSeed);
        }
    }
}

void JigsawStructureGenerator::addBeard(StructureBeard& beard, std::int32_t chunkX, std::int32_t chunkZ,
                                        std::int64_t seed) {
    // Filtered with the inflated box so a chunk without its own piece still gets the beard.
    const StructureBoundingBox nearClip = chunkClip(chunkX, chunkZ, kBeardRadius);
    const std::int32_t chunkMinX = chunkX * 16;
    const std::int32_t chunkMinZ = chunkZ * 16;
    std::vector<BeardifierRigid>& rigids = beard.pieces;
    std::vector<BeardifierJunction>& junctions = beard.junctions;
    for (const auto& [origin, pieces] : nearbyStarts(chunkX, chunkZ, seed, nearClip)) {
        for (const PlacedStructurePiece& piece : *pieces) {
            if (!piece.boundingBox.intersects(nearClip)) {
                continue;
            }
            // parity: a junction contributes the piece's own projection, without the rigid-only test.
            for (const structure::PlacedJigsawJunction& junction : piece.junctions) {
                if (junction.sourceX > chunkMinX - kBeardRadius && junction.sourceX < chunkMinX + 15 + kBeardRadius &&
                    junction.sourceZ > chunkMinZ - kBeardRadius && junction.sourceZ < chunkMinZ + 15 + kBeardRadius) {
                    junctions.push_back(BeardifierJunction{
                        .x = junction.sourceX, .groundY = junction.sourceGroundY, .z = junction.sourceZ});
                }
            }
            if (piece.element.projection() != structure::StructureProjection::Rigid) {
                continue;
            }
            rigids.push_back(BeardifierRigid{.box = piece.boundingBox,
                                             .groundLevelDelta = piece.groundLevelDelta,
                                             .adjustment = settings_.adjustment});
        }
    }
}

std::optional<std::pair<std::int32_t, std::int32_t>>
JigsawStructureGenerator::locate(std::int64_t seed, std::int32_t fromChunkX, std::int32_t fromChunkZ,
                                 std::int32_t maxCells, const std::function<bool(std::int32_t, std::int32_t)>& skip) {
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
                const std::shared_ptr<const std::vector<PlacedStructurePiece>> assembled =
                    assembledStart(candidateX, candidateZ, seed);
                if (assembled == nullptr || assembled->empty() || (skip && skip(candidateX, candidateZ))) {
                    continue;
                }
                return std::pair{candidateX, candidateZ};
            }
        }
    }
    return std::nullopt;
}

} // namespace basalt::server::world::terrain
