#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/terrain/MineshaftStructureGenerator.hpp>

#include <array>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

constexpr float kFrequency = 0.004F;
constexpr std::int32_t kUndergroundStructuresStep = 3;
/// A shaft reaches 80 blocks from its room, so six chunks out covers every piece.
constexpr std::int32_t kReachChunks = 6;
constexpr std::size_t kCacheCapacity = 64;

/// Second and third of the five structures of `underground_structures`, in registry order.
constexpr std::array<std::pair<std::int32_t, structure::MineshaftType>, 2> kEntries{
    std::pair{1, structure::MineshaftType::Normal}, std::pair{2, structure::MineshaftType::Mesa}};

} // namespace

MineshaftStructureGenerator::MineshaftStructureGenerator(BiomeAt biomeAt,
                                                         std::set<std::string, std::less<>> normalBiomes,
                                                         std::set<std::string, std::less<>> mesaBiomes,
                                                         structure::mineshaftLayout::SurfaceHeightAt surfaceHeightAt,
                                                         std::int32_t seaLevel)
    : biomeAt_(std::move(biomeAt)), normalBiomes_(std::move(normalBiomes)), mesaBiomes_(std::move(mesaBiomes)),
      surfaceHeightAt_(std::move(surfaceHeightAt)), seaLevel_(seaLevel),
      grid_(1, 0, 0, structure::SpreadType::Linear, kFrequency, structure::FrequencyReduction::LegacyType3) {}

bool MineshaftStructureGenerator::allows(structure::MineshaftType type, const std::string& biome) const {
    const std::set<std::string, std::less<>>& biomes =
        type == structure::MineshaftType::Mesa ? mesaBiomes_ : normalBiomes_;
    return biomes.contains(biome);
}

std::optional<structure::MineshaftType> MineshaftStructureGenerator::typeAt(std::int64_t seed, std::int32_t chunkX,
                                                                            std::int32_t chunkZ) const {
    if (!grid_.isStructureChunk(seed, chunkX, chunkZ)) {
        return std::nullopt;
    }
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, chunkX, chunkZ);
    const structure::MineshaftType first =
        random.nextInt(2) == 0 ? structure::MineshaftType::Normal : structure::MineshaftType::Mesa;
    if (accepts(seed, chunkX, chunkZ, first)) {
        return first;
    }
    const structure::MineshaftType second =
        first == structure::MineshaftType::Normal ? structure::MineshaftType::Mesa : structure::MineshaftType::Normal;
    return accepts(seed, chunkX, chunkZ, second) ? std::optional{second} : std::nullopt;
}

bool MineshaftStructureGenerator::accepts(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                                          structure::MineshaftType type) const {
    // parity: the biome is read at the structure's own start, not at sea level, and vanilla builds
    // that position out of the chunk's middle x but its minimum z. A shaft that ends up in a cave
    // biome is kept on that biome's tag even when the surface above it is ocean.
    const structure::mineshaftLayout::Assembly& assembly = assemblyOf(seed, chunkX, chunkZ, type);
    return allows(type, biomeAt_(chunkX * 16 + 8, assembly.stubY, chunkZ * 16));
}

const structure::mineshaftLayout::Assembly&
MineshaftStructureGenerator::assemblyOf(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                                        structure::MineshaftType type) const {
    const std::pair<std::int64_t, structure::MineshaftType> key{
        (static_cast<std::int64_t>(chunkX) << 32) | (static_cast<std::int64_t>(chunkZ) & 0xFFFFFFFFLL), type};
    {
        const std::scoped_lock lock{mutex_};
        if (const auto found = assembled_.find(key); found != assembled_.end()) {
            return found->second;
        }
    }
    structure::mineshaftLayout::Assembly assembly = structure::mineshaftLayout::assemble(
        seed, chunkX, chunkZ, type, seaLevel_, kOverworldHeight.minY(), surfaceHeightAt_);
    const std::scoped_lock lock{mutex_};
    if (const auto found = assembled_.find(key); found != assembled_.end()) {
        return found->second;
    }
    if (assembled_.size() >= kCacheCapacity) {
        assembled_.clear();
    }
    return assembled_.emplace(key, std::move(assembly)).first->second;
}

void MineshaftStructureGenerator::generateInto(Chunk& chunk, std::int64_t seed) {
    const structure::StructureBoundingBox clip{chunk.chunkX() * 16,     kOverworldHeight.minY(),
                                               chunk.chunkZ() * 16,     chunk.chunkX() * 16 + 15,
                                               kOverworldHeight.maxY(), chunk.chunkZ() * 16 + 15};
    // parity: the two mineshafts are separate entries of `underground_structures`, so each gets its
    // own feature seed and one random shared by every start of that entry in this chunk. Seeding per
    // start, or off `setLargeFeatureSeed`, puts every roll inside the shaft (rails, cobwebs,
    // torches) on a stream vanilla never uses.
    for (const auto& [index, wanted] : kEntries) {
        random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
        const std::int64_t decorationSeed =
            random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
        random::setFeatureSeed(random, decorationSeed, index, kUndergroundStructuresStep);
        for (std::int32_t startX = chunk.chunkX() - kReachChunks; startX <= chunk.chunkX() + kReachChunks; ++startX) {
            for (std::int32_t startZ = chunk.chunkZ() - kReachChunks; startZ <= chunk.chunkZ() + kReachChunks;
                 ++startZ) {
                const std::optional<structure::MineshaftType> type = typeAt(seed, startX, startZ);
                if (!type.has_value() || *type != wanted) {
                    continue;
                }
                const std::vector<structure::MineshaftPiece>& pieces = assemblyOf(seed, startX, startZ, *type).pieces;
                const structure::MineshaftBlocks blocks{*type};
                for (const structure::MineshaftPiece& piece : pieces) {
                    if (!piece.box.intersects(clip)) {
                        continue;
                    }
                    structure::mineshaftFill::place(piece, blocks, chunk, clip, random, biomeAt_);
                }
            }
        }
    }
}

} // namespace basalt::server::world::terrain
