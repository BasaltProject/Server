#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/terrain/DesertPyramidStructureGenerator.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <tuple>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain {
namespace {

using structure::StructureBoundingBox;
using structure::StructureDirection;
using structure::StructurePos;

constexpr std::int32_t kSurfaceStructuresStep = 4;
constexpr std::string_view kBiome = "desert";
constexpr std::int32_t kReachChunks = 2;
constexpr std::int32_t kGroundOffsetBound = 3;
constexpr std::int32_t kSuspiciousMin = 5;
constexpr std::int32_t kSuspiciousSpread = 3;

constexpr std::int64_t kXMask = 0x3FFFFFFLL;
constexpr std::int64_t kZMask = 0x3FFFFFFLL;
constexpr std::int64_t kYMask = 0xFFFLL;
constexpr std::int32_t kXShift = 38;
constexpr std::int32_t kZShift = 12;

/// parity: `Direction.Plane.HORIZONTAL`, the array `getRandomDirection` draws from.
constexpr std::array<StructureDirection, 4> kHorizontal{StructureDirection::North, StructureDirection::East,
                                                        StructureDirection::South, StructureDirection::West};

[[nodiscard]] std::int32_t state(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

[[nodiscard]] std::int32_t sand() {
    static const std::int32_t id = state("sand");
    return id;
}

[[nodiscard]] std::int32_t suspiciousSand() {
    static const std::int32_t id = state("suspicious_sand");
    return id;
}

/// parity: `BlockPos.asLong`, which is what seeds a brushable block's loot.
[[nodiscard]] std::int64_t asLong(StructurePos position) {
    return ((static_cast<std::int64_t>(position.x) & kXMask) << kXShift) |
           (static_cast<std::int64_t>(position.y) & kYMask) |
           ((static_cast<std::int64_t>(position.z) & kZMask) << kZShift);
}

[[nodiscard]] StructureBoundingBox chunkClip(std::int32_t chunkX, std::int32_t chunkZ) {
    return StructureBoundingBox{chunkX * 16,      kOverworldHeight.minY(), chunkZ * 16,
                                chunkX * 16 + 15, kOverworldHeight.maxY(), chunkZ * 16 + 15};
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

[[nodiscard]] std::int32_t floorDiv(std::int32_t value, std::int32_t divisor) {
    const std::int32_t quotient = value / divisor;
    return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
}

} // namespace

DesertPyramidStructureGenerator::DesertPyramidStructureGenerator(BiomeAt biomeAt, SurfaceHeightAt surfaceHeightAt)
    : biomeAt_(std::move(biomeAt)), surfaceHeightAt_(std::move(surfaceHeightAt)) {}

const structure::StructurePlacementGrid& DesertPyramidStructureGenerator::grid() {
    static const structure::StructurePlacementGrid built{32, 8, 14357617};
    return built;
}

std::int32_t DesertPyramidStructureGenerator::groundOffset(std::int64_t seed, std::int32_t chunkX,
                                                           std::int32_t chunkZ) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed = random::setDecorationSeed(random, seed, chunkX * 16, chunkZ * 16);
    random::setFeatureSeed(random, decorationSeed, kStructureIndex, kSurfaceStructuresStep);
    return random.nextInt(kGroundOffsetBound);
}

void DesertPyramidStructureGenerator::setSuspiciousSand(Chunk& chunk, const StructureBoundingBox& clip,
                                                        StructurePos position) {
    if (!clip.isInside(position)) {
        return;
    }
    if (position.y < kOverworldHeight.minY() || position.y > kOverworldHeight.maxY()) {
        return;
    }
    chunk.setBlockStateId(position.x & 15, position.y, position.z & 15, suspiciousSand());
    wire::nbt::NbtCompound data;
    data.set("LootTable", wire::nbt::NbtTag::ofString("minecraft:archaeology/desert_pyramid"));
    data.set("LootTableSeed", wire::nbt::NbtTag::ofLong(asLong(position)));
    data.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
    chunk.setBlockEntity(ChunkBlockEntity{position.x & 15, position.y, position.z & 15,
                                          wire::common::Identifier::vanilla("brushable_block"), std::move(data)});
}

void DesertPyramidStructureGenerator::placeSuspiciousSand(Chunk& chunk, const StructureBoundingBox& clip,
                                                          std::int64_t seed, const DesertPyramidPiece& piece) {
    if (piece.collapsedRoofPos().has_value()) {
        setSuspiciousSand(chunk, clip, *piece.collapsedRoofPos());
    }
    // parity: a `SortedArraySet` over `Vec3i`, which orders by y, then z, then x.
    std::vector<StructurePos> unique = piece.suspiciousSandPositions();
    std::sort(unique.begin(), unique.end(), [](StructurePos first, StructurePos second) {
        return std::tie(first.y, first.z, first.x) < std::tie(second.y, second.z, second.x);
    });
    unique.erase(std::unique(unique.begin(), unique.end()), unique.end());

    const StructureBoundingBox& bounds = piece.bounds();
    const StructurePos centre{bounds.minX + (bounds.maxX - bounds.minX + 1) / 2,
                              bounds.minY + (bounds.maxY - bounds.minY + 1) / 2,
                              bounds.minZ + (bounds.maxZ - bounds.minZ + 1) / 2};
    random::RandomSource random = random::RandomSource::legacy(seed).forkPositional().at(centre.x, centre.y, centre.z);
    // parity: `Util.shuffle`, which walks from the end and swaps into the tail.
    for (std::size_t i = unique.size(); i >= 2; --i) {
        const std::size_t swapTo = static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(i)));
        std::swap(unique[i - 1], unique[swapTo]);
    }
    std::int32_t remaining =
        std::min(static_cast<std::int32_t>(unique.size()), kSuspiciousMin + random.nextInt(kSuspiciousSpread));
    for (const StructurePos& position : unique) {
        if (remaining > 0) {
            --remaining;
            setSuspiciousSand(chunk, clip, position);
        } else if (clip.isInside(position)) {
            chunk.setBlockStateId(position.x & 15, position.y, position.z & 15, sand());
        }
    }
}

void DesertPyramidStructureGenerator::placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX,
                                                std::int32_t chunkZ) const {
    if (!grid().isStructureChunk(seed, chunkX, chunkZ)) {
        return;
    }

    // parity: `Structure.GenerationContext`'s own random, which is what the orientation comes off.
    random::RandomSource startRandom = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(startRandom, seed, chunkX, chunkZ);

    const std::int32_t centreX = chunkX * 16 + 8;
    const std::int32_t centreZ = chunkZ * 16 + 8;
    const std::int32_t stubY = surfaceHeightAt_(centreX, centreZ) - 1;
    if (biomePath(biomeAt_(centreX, stubY, centreZ)) != kBiome) {
        return;
    }

    const StructureDirection orientation =
        kHorizontal[static_cast<std::size_t>(startRandom.nextInt(static_cast<std::int32_t>(kHorizontal.size())))];
    DesertPyramidPiece piece{chunkX * 16, chunkZ * 16, orientation, -groundOffset(seed, chunkX, chunkZ)};
    if (!piece.settleOnGround(surfaceHeightAt_)) {
        return;
    }

    const StructureBoundingBox clip = chunkClip(chunk.chunkX(), chunk.chunkZ());
    if (!piece.bounds().intersects(clip)) {
        return;
    }

    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kStructureIndex, kSurfaceStructuresStep);
    // parity: the ground offset is drawn once per placing chunk whether or not it is used.
    static_cast<void>(random.nextInt(kGroundOffsetBound));

    piece.place(chunk, clip, seed, random);
    placeSuspiciousSand(chunk, clip, seed, piece);
    chunk.structures().add(structure::StructureStartRecord{.structure = std::string{kName},
                                                           .chunkX = chunkX,
                                                           .chunkZ = chunkZ,
                                                           .fullBounds = piece.bounds(),
                                                           .pieces = {piece.bounds()}});
}

void DesertPyramidStructureGenerator::generateInto(Chunk& chunk, std::int64_t seed) {
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
