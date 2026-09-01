#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/terrain/StructureBeardifier.hpp>
#include <basalt/server/world/terrain/VillageStructureGenerator.hpp>

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

using structure::PlacedStructurePiece;
using structure::StructureBoundingBox;
using structure::StructurePos;

constexpr std::int32_t kCellWidth = 4;
constexpr std::int32_t kCellHeight = 8;
constexpr std::int32_t kSeaLevel = 63;
constexpr std::int32_t kBeardRadius = 12;
constexpr std::int32_t kSurfaceStructuresStep = 4;

/// 64 candidate cells is over 2000 chunks of travel at the villages grid spacing; a full re-assembly
/// after an overflow costs about a second per nearby candidate.
constexpr std::size_t kAssemblyCacheCapacity = 64;

[[nodiscard]] StructureBoundingBox chunkClip(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t margin) {
    return StructureBoundingBox{chunkX * 16 - margin,      kOverworldHeight.minY(), chunkZ * 16 - margin,
                                chunkX * 16 + 15 + margin, kOverworldHeight.maxY(), chunkZ * 16 + 15 + margin};
}

[[nodiscard]] std::int32_t floorDiv(std::int32_t value, std::int32_t divisor) {
    const std::int32_t quotient = value / divisor;
    return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
}

/// The biome name without its namespace, which is what the village tags are written against.
[[nodiscard]] std::string withoutNamespace(std::string_view biome) {
    const std::size_t colon = biome.find(':');
    return std::string{colon == std::string_view::npos ? biome : biome.substr(colon + 1)};
}

} // namespace

VillageStructureGenerator::VillageStructureGenerator(OverworldNoiseRouter router,
                                                     structure::StructurePlacementGrid grid, BiomeAt biomeAt,
                                                     std::int32_t maxDepth, std::int32_t maxDistanceFromCenter,
                                                     PoolLookup poolLookup,
                                                     structure::PoolTemplateLoader templateLoader)
    : router_(std::move(router)), grid_(std::move(grid)), biomeAt_(std::move(biomeAt)), maxDepth_(maxDepth),
      maxDistanceFromCenter_(maxDistanceFromCenter), poolLookup_(std::move(poolLookup)),
      templateLoader_(std::move(templateLoader)) {}

const std::vector<VillageStructureGenerator::Variant>& VillageStructureGenerator::variants() {
    static const std::vector<Variant> built{
        {wire::common::Identifier::vanilla("village_plains"),
         "minecraft:village/plains/town_centers",
         {"plains", "meadow"},
         22},
        {wire::common::Identifier::vanilla("village_desert"), "minecraft:village/desert/town_centers", {"desert"}, 21},
        {wire::common::Identifier::vanilla("village_savanna"),
         "minecraft:village/savanna/town_centers",
         {"savanna"},
         23},
        {wire::common::Identifier::vanilla("village_snowy"),
         "minecraft:village/snowy/town_centers",
         {"snowy_plains"},
         24},
        {wire::common::Identifier::vanilla("village_taiga"), "minecraft:village/taiga/town_centers", {"taiga"}, 25},
    };
    return built;
}

const std::vector<const VillageStructureGenerator::Variant*>& VillageStructureGenerator::placementVariants() {
    static const std::vector<const Variant*> built = [] {
        std::vector<const Variant*> all;
        all.reserve(variants().size());
        for (const Variant& variant : variants()) {
            all.push_back(&variant);
        }
        std::sort(all.begin(), all.end(), [](const Variant* first, const Variant* second) {
            return first->structureIndex < second->structureIndex;
        });
        return all;
    }();
    return built;
}

const std::vector<wire::common::Identifier>& VillageStructureGenerator::structureIds() {
    static const std::vector<wire::common::Identifier> built = [] {
        std::vector<wire::common::Identifier> all;
        all.reserve(variants().size());
        for (const Variant& variant : variants()) {
            all.push_back(variant.id);
        }
        return all;
    }();
    return built;
}

std::pair<std::size_t, std::int64_t> VillageStructureGenerator::assemblyStats() const {
    const std::scoped_lock lock{mutex_};
    std::int64_t pieces = 0;
    for (const auto& [key, start] : cache_) {
        pieces += static_cast<std::int64_t>(start.pieces.size());
    }
    return {cache_.size(), pieces};
}

std::int32_t VillageStructureGenerator::surfaceHeightAt(std::int32_t x, std::int32_t z, Samplers& samplers) const {
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
    std::int32_t y = kOverworldHeight.maxY();
    while (y >= kOverworldHeight.minY() && found->second.sample(x & 15, y, z & 15) <= 0.0) {
        --y;
    }
    // parity: `NoiseBasedChunkGenerator.getFirstFreeHeight` samples the generated base column, not
    // density alone. The Overworld's global aquifer fills open columns through y 62, so
    // `WORLD_SURFACE_WG` is at least sea level even when the solid ocean floor is lower.
    return std::max(y + 1, kSeaLevel);
}

const VillageStructureGenerator::Variant* VillageStructureGenerator::selectVariant(std::int32_t candidateChunkX,
                                                                                   std::int32_t candidateChunkZ,
                                                                                   std::int64_t seed,
                                                                                   Samplers& samplers) const {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, candidateChunkX, candidateChunkZ);
    const std::int32_t centreX = candidateChunkX * 16 + 8;
    const std::int32_t centreZ = candidateChunkZ * 16 + 8;
    const std::string biome = withoutNamespace(biomeAt_(centreX, surfaceHeightAt(centreX, centreZ, samplers), centreZ));
    std::vector<const Variant*> options;
    options.reserve(variants().size());
    for (const Variant& variant : variants()) {
        options.push_back(&variant);
    }
    std::int32_t total = static_cast<std::int32_t>(options.size());
    while (!options.empty()) {
        // Every real entry weighs one, so the draw walks the list subtracting that weight.
        std::int32_t choice = random.nextInt(total);
        std::size_t index = 0;
        for (std::size_t candidate = 0; candidate < options.size(); ++candidate) {
            choice -= 1;
            if (choice < 0) {
                break;
            }
            ++index;
        }
        const Variant* variant = options[index];
        if (variant->biomes.contains(biome)) {
            return variant;
        }
        options.erase(options.begin() + static_cast<std::ptrdiff_t>(index));
        total -= 1;
    }
    return nullptr;
}

std::vector<PlacedStructurePiece> VillageStructureGenerator::assemble(std::string_view startPool, std::int32_t chunkX,
                                                                      std::int32_t chunkZ, std::int32_t maxDepth,
                                                                      std::int64_t seed, Samplers& samplers,
                                                                      random::RandomSource& random) const {
    static_cast<void>(seed);
    const structure::JigsawWorld world{
        .poolLookup = poolLookup_,
        .templateLoader = templateLoader_,
        .heightAt = [this, &samplers](std::int32_t x, std::int32_t z) { return surfaceHeightAt(x, z, samplers); }};
    return structure::assembleJigsaw(
        world,
        structure::JigsawRequest{.startPool = std::string{startPool},
                                 .startPosition = StructurePos{chunkX * 16, 0, chunkZ * 16},
                                 .maxDepth = maxDepth,
                                 .projectStartToHeightmap = true,
                                 .maxDistanceFromCenter = maxDistanceFromCenter_,
                                 .useExpansionHack = true},
        random);
}

const VillageStructureGenerator::VillageStart* VillageStructureGenerator::assembledStart(std::int32_t candidateChunkX,
                                                                                         std::int32_t candidateChunkZ,
                                                                                         std::int64_t seed) {
    const CandidateKey key{seed, candidateChunkX, candidateChunkZ};
    {
        const std::scoped_lock lock{mutex_};
        const auto found = cache_.find(key);
        if (found != cache_.end()) {
            return found->second.variant == nullptr ? nullptr : &found->second;
        }
    }

    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, candidateChunkX, candidateChunkZ);
    Samplers samplers;
    VillageStart built;
    built.variant = selectVariant(candidateChunkX, candidateChunkZ, seed, samplers);
    if (built.variant != nullptr) {
        built.pieces =
            assemble(built.variant->startPool, candidateChunkX, candidateChunkZ, maxDepth_, seed, samplers, random);
    }

    const std::scoped_lock lock{mutex_};
    const auto found = cache_.find(key);
    if (found != cache_.end()) {
        return found->second.variant == nullptr ? nullptr : &found->second;
    }
    if (cache_.size() >= kAssemblyCacheCapacity) {
        cache_.clear();
    }
    const VillageStart& stored = cache_.emplace(key, std::move(built)).first->second;
    return stored.variant == nullptr ? nullptr : &stored;
}

std::vector<const VillageStructureGenerator::VillageStart*>
VillageStructureGenerator::referencedStarts(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed,
                                            const StructureBoundingBox& clip) {
    const std::int32_t cellX = floorDiv(chunkX, grid_.spacing());
    const std::int32_t cellZ = floorDiv(chunkZ, grid_.spacing());
    const std::int32_t reachChunks = (maxDistanceFromCenter_ + kBeardRadius) / 16 + 1;
    std::vector<const VillageStart*> starts;
    for (std::int32_t dx = -1; dx <= 1; ++dx) {
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            const auto [candidateChunkX, candidateChunkZ] =
                grid_.candidateChunk(seed, (cellX + dx) * grid_.spacing(), (cellZ + dz) * grid_.spacing());
            if (std::abs(candidateChunkX - chunkX) > reachChunks || std::abs(candidateChunkZ - chunkZ) > reachChunks) {
                continue;
            }
            const VillageStart* start = assembledStart(candidateChunkX, candidateChunkZ, seed);
            if (start == nullptr) {
                continue;
            }
            const bool touches =
                std::any_of(start->pieces.begin(), start->pieces.end(),
                            [&clip](const PlacedStructurePiece& piece) { return piece.boundingBox.intersects(clip); });
            if (touches) {
                starts.push_back(start);
            }
        }
    }
    return starts;
}

void VillageStructureGenerator::generateInto(Chunk& chunk, std::int32_t chunkX, std::int32_t chunkZ,
                                             std::int64_t seed) {
    const StructureBoundingBox clip = chunkClip(chunkX, chunkZ, 0);
    const std::vector<const VillageStart*> starts = referencedStarts(chunkX, chunkZ, seed, clip);
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed = random::setDecorationSeed(random, seed, chunkX * 16, chunkZ * 16);
    for (const Variant* variant : placementVariants()) {
        random::setFeatureSeed(random, decorationSeed, variant->structureIndex, kSurfaceStructuresStep);
        for (const VillageStart* start : starts) {
            if (start->variant != variant) {
                continue;
            }
            for (const PlacedStructurePiece& piece : start->pieces) {
                if (!piece.boundingBox.intersects(clip)) {
                    continue;
                }
                piece.element.place(templateLoader_, chunk, piece.position, piece.rotation, clip, random);
            }
        }
    }
}

std::optional<LocatedVillage> VillageStructureGenerator::locate(std::int64_t seed, std::int32_t fromChunkX,
                                                                std::int32_t fromChunkZ,
                                                                const wire::common::Identifier* structure,
                                                                std::int32_t maxCells) const {
    const std::int32_t cellX = floorDiv(fromChunkX, grid_.spacing());
    const std::int32_t cellZ = floorDiv(fromChunkZ, grid_.spacing());
    for (std::int32_t ring = 0; ring <= maxCells; ++ring) {
        for (std::int32_t dx = -ring; dx <= ring; ++dx) {
            for (std::int32_t dz = -ring; dz <= ring; ++dz) {
                // Only the ring itself: the inside was searched by a previous, smaller one.
                if (ring != 0 && std::abs(dx) != ring && std::abs(dz) != ring) {
                    continue;
                }
                const auto [candidateX, candidateZ] =
                    grid_.candidateChunk(seed, (cellX + dx) * grid_.spacing(), (cellZ + dz) * grid_.spacing());
                Samplers samplers;
                const Variant* variant = selectVariant(candidateX, candidateZ, seed, samplers);
                if (variant == nullptr) {
                    continue;
                }
                if (structure != nullptr && variant->id != *structure) {
                    continue;
                }
                return LocatedVillage{.structure = variant->id, .chunkX = candidateX, .chunkZ = candidateZ};
            }
        }
    }
    return std::nullopt;
}

std::optional<std::vector<PlacedStructurePiece>>
VillageStructureGenerator::assembleAt(const wire::common::Identifier& structure, std::int32_t chunkX,
                                      std::int32_t chunkZ, std::int64_t seed) const {
    const Variant* found = nullptr;
    for (const Variant& variant : variants()) {
        if (variant.id == structure) {
            found = &variant;
            break;
        }
    }
    if (found == nullptr) {
        return std::nullopt;
    }
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, chunkX, chunkZ);
    Samplers samplers;
    return assemble(found->startPool, chunkX, chunkZ, maxDepth_, seed, samplers, random);
}

std::vector<PlacedStructurePiece> VillageStructureGenerator::assemblePool(std::string_view pool, std::int32_t chunkX,
                                                                          std::int32_t chunkZ, std::int32_t maxDepth,
                                                                          std::int64_t seed) const {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, chunkX, chunkZ);
    Samplers samplers;
    return assemble(pool, chunkX, chunkZ, maxDepth, seed, samplers, random);
}

void VillageStructureGenerator::placeInto(Chunk& chunk, const std::vector<PlacedStructurePiece>& pieces,
                                          std::int64_t seed) const {
    const StructureBoundingBox clip = chunkClip(chunk.chunkX(), chunk.chunkZ(), 0);
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    for (const PlacedStructurePiece& piece : pieces) {
        if (!piece.boundingBox.intersects(clip)) {
            continue;
        }
        piece.element.place(templateLoader_, chunk, piece.position, piece.rotation, clip, random);
    }
}

densityfunction::Function VillageStructureGenerator::finalDensityForChunk(std::int32_t chunkX, std::int32_t chunkZ,
                                                                          std::int64_t seed) {
    const std::int32_t chunkMinX = chunkX * 16;
    const std::int32_t chunkMinZ = chunkZ * 16;
    const StructureBoundingBox referenceClip = chunkClip(chunkX, chunkZ, 0);
    const StructureBoundingBox nearClip = chunkClip(chunkX, chunkZ, kBeardRadius);
    std::vector<BeardifierRigid> rigids;
    std::vector<BeardifierJunction> junctions;
    for (const VillageStart* start : referencedStarts(chunkX, chunkZ, seed, referenceClip)) {
        for (const PlacedStructurePiece& piece : start->pieces) {
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
        return router_.finalDensity;
    }
    return densityfunction::functions::add(router_.finalDensity,
                                           structureBeardifier(std::move(rigids), std::move(junctions)));
}

} // namespace basalt::server::world::terrain
