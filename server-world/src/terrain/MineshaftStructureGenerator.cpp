#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/terrain/MineshaftStructureGenerator.hpp>
#include <basalt/server/world/terrain/StructureReferenceOrder.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain {
namespace {

constexpr float kFrequency = 0.004F;
constexpr std::int32_t kUndergroundStructuresStep = 3;
/// A shaft reaches 80 blocks from its room, so six chunks out covers every piece.
constexpr std::int32_t kReachChunks = 6;
constexpr std::size_t kCacheCapacity = 64;

/// Second and third of the five structures of `underground_structures`, in registry order.
/// parity: the two are separate structures in the registry, and a start says which one it is.
constexpr std::string_view kNormalName = "mineshaft";
constexpr std::string_view kMesaName = "mineshaft_mesa";

constexpr std::array<std::pair<std::int32_t, structure::MineshaftType>, 2> kEntries{
    std::pair{1, structure::MineshaftType::Normal}, std::pair{2, structure::MineshaftType::Mesa}};

} // namespace

MineshaftStructureGenerator::MineshaftStructureGenerator(BiomeAt biomeAt,
                                                         std::set<std::string, std::less<>> normalBiomes,
                                                         std::set<std::string, std::less<>> mesaBiomes,
                                                         structure::mineshaftLayout::SurfaceHeightAt surfaceHeightAt,
                                                         std::int32_t seaLevel, BiomeAt startBiomeAt)
    : biomeAt_(std::move(biomeAt)), startBiomeAt_(std::move(startBiomeAt)), normalBiomes_(std::move(normalBiomes)),
      mesaBiomes_(std::move(mesaBiomes)), surfaceHeightAt_(std::move(surfaceHeightAt)), seaLevel_(seaLevel),
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
    const std::shared_ptr<const structure::mineshaftLayout::Assembly> assembly = assemblyOf(seed, chunkX, chunkZ, type);
    const BiomeAt& biomeAt = startBiomeAt_ ? startBiomeAt_ : biomeAt_;
    return allows(type, biomeAt(chunkX * 16 + 8, assembly->stubY, chunkZ * 16));
}

std::shared_ptr<const structure::mineshaftLayout::Assembly>
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
    auto assembly = std::make_shared<const structure::mineshaftLayout::Assembly>(structure::mineshaftLayout::assemble(
        seed, chunkX, chunkZ, type, seaLevel_, kOverworldHeight.minY(), surfaceHeightAt_));
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
        // parity: `StructureManager.startsForStructure` walks the chunk's references, a `LongOpenHashSet`
        // filled x outermost with every start whose box reaches the chunk. Its hash order decides which
        // shaft draws from the shared random first.
        std::vector<std::pair<std::int32_t, std::int32_t>> references;
        for (std::int32_t startX = chunk.chunkX() - kReachChunks; startX <= chunk.chunkX() + kReachChunks; ++startX) {
            for (std::int32_t startZ = chunk.chunkZ() - kReachChunks; startZ <= chunk.chunkZ() + kReachChunks;
                 ++startZ) {
                const std::optional<structure::MineshaftType> type = typeAt(seed, startX, startZ);
                if (!type.has_value() || *type != wanted) {
                    continue;
                }
                const std::vector<structure::MineshaftPiece>& pieces = assemblyOf(seed, startX, startZ, *type)->pieces;
                if (pieces.empty()) {
                    continue;
                }
                structure::StructureBoundingBox whole = pieces.front().box;
                for (const structure::MineshaftPiece& piece : pieces) {
                    whole = whole.encapsulating(piece.box);
                }
                if (whole.minX <= clip.maxX && whole.maxX >= clip.minX && whole.minZ <= clip.maxZ &&
                    whole.maxZ >= clip.minZ) {
                    references.emplace_back(startX, startZ);
                }
            }
        }
        for (const auto& [startX, startZ] : vanillaStructureReferenceChunkOrder(references)) {
            {
                const std::optional<structure::MineshaftType> type = typeAt(seed, startX, startZ);
                const std::shared_ptr<const structure::mineshaftLayout::Assembly> assembly =
                    assemblyOf(seed, startX, startZ, *type);
                const std::vector<structure::MineshaftPiece>& pieces = assembly->pieces;
                const structure::MineshaftBlocks blocks{*type};
                std::vector<structure::StructureBoundingBox> boxes;
                boxes.reserve(pieces.size());
                bool reachesHere = false;
                for (const structure::MineshaftPiece& piece : pieces) {
                    boxes.push_back(piece.box);
                    if (!piece.box.intersects(clip)) {
                        continue;
                    }
                    reachesHere = true;
                    structure::mineshaftFill::place(piece, blocks, chunk, clip, random, biomeAt_);
                }
                // The blocks alone are not the whole structure: without the start the chunk cannot
                // say what it holds, so `/locate` never finds it and nothing knows a position is
                // inside one. parity: the start belongs to the chunk the shaft begins in.
                if (reachesHere) {
                    const std::string_view name = *type == structure::MineshaftType::Mesa ? kMesaName : kNormalName;
                    if (std::optional<structure::StructureStartRecord> start =
                            structure::StructureStartRecord::of(std::string{name}, startX, startZ, boxes)) {
                        // The full box is the whole shaft, but the pieces kept here are only the ones
                        // this chunk holds: every question a chunk is asked is about a position inside
                        // it, and a piece that misses the chunk cannot answer one. A shaft crosses
                        // dozens of chunks and carries some hundred and fifty pieces into each.
                        std::erase_if(start->pieces, [&clip](const structure::StructureBoundingBox& piece) {
                            return !piece.intersects(clip);
                        });
                        chunk.structures().add(std::move(*start));
                    }
                }
            }
        }
    }
}

std::optional<structure::StructureBoundingBox>
MineshaftStructureGenerator::placeBoundsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                                           structure::MineshaftType type) const {
    const std::shared_ptr<const structure::mineshaftLayout::Assembly> assembly = assemblyOf(seed, chunkX, chunkZ, type);
    if (assembly->pieces.empty()) {
        return std::nullopt;
    }
    structure::StructureBoundingBox whole = assembly->pieces.front().box;
    for (const structure::MineshaftPiece& piece : assembly->pieces) {
        whole = whole.encapsulating(piece.box);
    }
    return whole;
}

void MineshaftStructureGenerator::placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                                          structure::MineshaftType type) const {
    const structure::StructureBoundingBox clip{chunk.chunkX() * 16,     kOverworldHeight.minY(),
                                               chunk.chunkZ() * 16,     chunk.chunkX() * 16 + 15,
                                               kOverworldHeight.maxY(), chunk.chunkZ() * 16 + 15};
    // parity: the placed shaft draws from the same per-chunk, per-entry stream worldgen gives a
    // chunk that holds it, so its content matches a naturally generated one.
    std::int32_t index = 1;
    for (const auto& [entryIndex, entryType] : kEntries) {
        if (entryType == type) {
            index = entryIndex;
        }
    }
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, index, kUndergroundStructuresStep);
    const std::shared_ptr<const structure::mineshaftLayout::Assembly> assembly = assemblyOf(seed, chunkX, chunkZ, type);
    const structure::MineshaftBlocks blocks{type};
    for (const structure::MineshaftPiece& piece : assembly->pieces) {
        if (piece.box.intersects(clip)) {
            structure::mineshaftFill::place(piece, blocks, chunk, clip, random, biomeAt_);
        }
    }
    // parity: `PlaceCommand.placeStructure` writes the pieces but registers no start.
}

} // namespace basalt::server::world::terrain
