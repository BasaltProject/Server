#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/terrain/EndCityStructureGenerator.hpp>
#include <basalt/server/world/terrain/StructureReferenceOrder.hpp>

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

using structure::StructureBoundingBox;
using structure::StructureMirror;
using structure::StructurePos;
using structure::StructureRotation;

constexpr std::int32_t kEndGenerationHeight = 128;
constexpr std::int32_t kMinStartY = 60;
constexpr std::int32_t kStructureReferenceRange = 8;
constexpr std::int32_t kEndCityStructureIndex = 2;
constexpr std::int32_t kSurfaceStructuresStep = 4;
constexpr std::string_view kEndCityLootTable = "minecraft:chests/end_city_treasure";
constexpr StructurePos kZero{0, 0, 0};

constexpr std::array<StructureRotation, 4> kRotations{StructureRotation::None, StructureRotation::Clockwise90,
                                                      StructureRotation::Clockwise180,
                                                      StructureRotation::Counterclockwise90};

[[nodiscard]] bool isEndCityBiome(std::string_view biome) {
    const std::size_t colon = biome.find(':');
    const std::string_view path = colon == std::string_view::npos ? biome : biome.substr(colon + 1);
    return path == "end_highlands" || path == "end_midlands";
}

[[nodiscard]] std::pair<std::int32_t, std::int32_t> heightOffsets(StructureRotation rotation) {
    switch (rotation) {
    case StructureRotation::Clockwise90:
        return {-5, 5};
    case StructureRotation::Clockwise180:
        return {-5, -5};
    case StructureRotation::Counterclockwise90:
        return {5, -5};
    case StructureRotation::None:
    default:
        return {5, 5};
    }
}

[[nodiscard]] StructureBoundingBox unionOf(const std::vector<endCityPieces::Piece>& pieces) {
    StructureBoundingBox result = pieces.front().boundingBox;
    for (std::size_t index = 1; index < pieces.size(); ++index) {
        const StructureBoundingBox& box = pieces[index].boundingBox;
        result = StructureBoundingBox{std::min(result.minX, box.minX), std::min(result.minY, box.minY),
                                      std::min(result.minZ, box.minZ), std::max(result.maxX, box.maxX),
                                      std::max(result.maxY, box.maxY), std::max(result.maxZ, box.maxZ)};
    }
    return result;
}

[[nodiscard]] StructureBoundingBox chunkClip(const Chunk& chunk) {
    return StructureBoundingBox{chunk.chunkX() * 16,      chunk.verticalBounds().minY(), chunk.chunkZ() * 16,
                                chunk.chunkX() * 16 + 15, chunk.verticalBounds().maxY(), chunk.chunkZ() * 16 + 15};
}

[[nodiscard]] std::int32_t floorDiv(std::int32_t value, std::int32_t divisor) {
    const std::int32_t quotient = value / divisor;
    return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
}

} // namespace

EndCityStructureGenerator::EndCityStructureGenerator(EndNoiseRouter router, const TheEndBiomeSource& biomeSource,
                                                     structure::TemplateLoader templateLoader,
                                                     SurfaceHeightOverride surfaceHeightOverride)
    : router_(std::move(router)), biomeSource_(&biomeSource), templateLoader_(std::move(templateLoader)),
      surfaceHeightOverride_(std::move(surfaceHeightOverride)) {}

const structure::StructurePlacementGrid& EndCityStructureGenerator::grid() {
    static const structure::StructurePlacementGrid built{20, 11, 10387313, structure::SpreadType::Triangular};
    return built;
}

DensityChunkSampler& EndCityStructureGenerator::samplerAt(std::int32_t x, std::int32_t z) {
    const std::pair<std::int32_t, std::int32_t> key{x >> 4, z >> 4};
    const auto found = samplers_.find(key);
    if (found != samplers_.end()) {
        return *found->second;
    }
    auto created = std::make_unique<DensityChunkSampler>(router_.finalDensity, key.first, key.second, 0,
                                                         kEndGenerationHeight, 8, 4);
    return *samplers_.emplace(key, std::move(created)).first->second;
}

std::int32_t EndCityStructureGenerator::surfaceHeightAt(std::int32_t x, std::int32_t z) {
    if (surfaceHeightOverride_) {
        return surfaceHeightOverride_(x, z);
    }
    DensityChunkSampler& sampler = samplerAt(x, z);
    for (std::int32_t y = kEndGenerationHeight - 1; y >= 0; --y) {
        if (sampler.sample(x & 15, y, z & 15) > 0.0) {
            return y;
        }
    }
    return -1;
}

const std::vector<endCityPieces::Piece>&
EndCityStructureGenerator::cityAt(std::int64_t seed, std::int32_t candidateChunkX, std::int32_t candidateChunkZ) {
    const std::scoped_lock lock{mutex_};
    const CityCacheKey key{seed, candidateChunkX, candidateChunkZ};
    const auto found = cities_.find(key);
    if (found != cities_.end()) {
        return found->second;
    }
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, candidateChunkX, candidateChunkZ);
    const StructureRotation rotation =
        kRotations[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kRotations.size())))];
    const std::int32_t startX = candidateChunkX * 16 + 7;
    const std::int32_t startZ = candidateChunkZ * 16 + 7;
    const auto [offsetX, offsetZ] = heightOffsets(rotation);
    const std::int32_t startY =
        std::min({surfaceHeightAt(startX, startZ), surfaceHeightAt(startX, startZ + offsetZ),
                  surfaceHeightAt(startX + offsetX, startZ), surfaceHeightAt(startX + offsetX, startZ + offsetZ)});
    const std::string_view biome = biomeSource_->biomeAtQuart(startX >> 2, startY >> 2, startZ >> 2);
    std::vector<endCityPieces::Piece> pieces;
    if (startY >= kMinStartY && isEndCityBiome(biome)) {
        pieces = endCityPieces::assemble(templateLoader_, StructurePos{startX, startY, startZ}, rotation, random);
    }
    return cities_.emplace(key, std::move(pieces)).first->second;
}

std::vector<EndCityStructureGenerator::CityReference>
EndCityStructureGenerator::referencedCities(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed,
                                            const StructureBoundingBox& clip) {
    const std::int32_t spacing = grid().spacing();
    const std::int32_t cellX = floorDiv(chunkX, spacing);
    const std::int32_t cellZ = floorDiv(chunkZ, spacing);
    std::vector<CityReference> references;
    for (std::int32_t cellOffsetX = -1; cellOffsetX <= 1; ++cellOffsetX) {
        for (std::int32_t cellOffsetZ = -1; cellOffsetZ <= 1; ++cellOffsetZ) {
            const auto [candidateX, candidateZ] =
                grid().candidateChunk(seed, (cellX + cellOffsetX) * spacing, (cellZ + cellOffsetZ) * spacing);
            if (candidateX < chunkX - kStructureReferenceRange || candidateX > chunkX + kStructureReferenceRange) {
                continue;
            }
            if (candidateZ < chunkZ - kStructureReferenceRange || candidateZ > chunkZ + kStructureReferenceRange) {
                continue;
            }
            const std::vector<endCityPieces::Piece>& pieces = cityAt(seed, candidateX, candidateZ);
            if (pieces.empty() || !unionOf(pieces).intersects(clip)) {
                continue;
            }
            references.push_back(CityReference{candidateX, candidateZ, &pieces});
        }
    }
    std::vector<std::pair<std::int32_t, std::int32_t>> insertionOrder;
    insertionOrder.reserve(references.size());
    for (const CityReference& reference : references) {
        insertionOrder.emplace_back(reference.chunkX, reference.chunkZ);
    }
    std::sort(insertionOrder.begin(), insertionOrder.end());
    std::vector<CityReference> ordered;
    ordered.reserve(references.size());
    for (const auto& [orderedX, orderedZ] : vanillaStructureReferenceChunkOrder(insertionOrder)) {
        for (const CityReference& reference : references) {
            if (reference.chunkX == orderedX && reference.chunkZ == orderedZ) {
                ordered.push_back(reference);
                break;
            }
        }
    }
    return ordered;
}

void EndCityStructureGenerator::generateInto(Chunk& chunk, std::int64_t seed) {
    const StructureBoundingBox clip = chunkClip(chunk);
    const std::vector<CityReference> references = referencedCities(chunk.chunkX(), chunk.chunkZ(), seed, clip);
    if (references.empty()) {
        return;
    }

    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kEndCityStructureIndex, kSurfaceStructuresStep);
    for (const CityReference& reference : references) {
        for (const endCityPieces::Piece& piece : *reference.pieces) {
            if (piece.piece == nullptr || !piece.boundingBox.intersects(clip)) {
                continue;
            }
            piece.piece->placeInto(chunk, structure::TemplatePlacement{.position = piece.position,
                                                                       .rotation = piece.rotation,
                                                                       .mirror = StructureMirror::None,
                                                                       .pivot = kZero,
                                                                       .clipTo = clip,
                                                                       .random = &random,
                                                                       .ignoreStructureBlocks = true,
                                                                       .ignoreAir = !piece.overwrite});
            handleMarkers(chunk, piece, clip, random);
        }
    }
}

void EndCityStructureGenerator::handleMarkers(Chunk& chunk, const endCityPieces::Piece& piece,
                                              const StructureBoundingBox& clip, random::RandomSource& random) const {
    for (const structure::StructureDataMarker& marker :
         piece.piece->dataMarkers(piece.position, piece.rotation, StructureMirror::None, kZero)) {
        const StructurePos position = marker.pos;
        if (marker.metadata.starts_with("Chest")) {
            const StructurePos chest{position.x, position.y - 1, position.z};
            if (!clip.isInside(chest)) {
                continue;
            }
            const ChunkBlockEntity* existing = chunk.blockEntityAt(chest.x & 15, chest.y, chest.z & 15);
            if (existing == nullptr) {
                continue;
            }
            wire::nbt::NbtCompound data = existing->data();
            data.set("LootTable", wire::nbt::NbtTag::ofString(std::string{kEndCityLootTable}));
            data.set("LootTableSeed", wire::nbt::NbtTag::ofLong(random.nextLong()));
            chunk.setBlockEntity(
                ChunkBlockEntity{chest.x & 15, chest.y, chest.z & 15, existing->type(), std::move(data)});
        } else if (marker.metadata.starts_with("Sentry") && clip.isInside(position)) {
            chunk.addGeneratedEntity(ChunkGeneratedEntity{.type = wire::common::Identifier::vanilla("shulker"),
                                                          .x = position.x + 0.5,
                                                          .y = static_cast<double>(position.y),
                                                          .z = position.z + 0.5,
                                                          .yaw = 0.0F,
                                                          .pitch = 0.0F});
        } else if (marker.metadata.starts_with("Elytra") && clip.isInside(position)) {
            placeElytraFrame(chunk, position, piece.rotation);
        }
    }
}

void EndCityStructureGenerator::placeElytraFrame(Chunk& chunk, StructurePos position, StructureRotation rotation) {
    std::string facing = "south";
    float yaw = 0.0F;
    switch (rotation) {
    case StructureRotation::Clockwise90:
        facing = "west";
        yaw = 90.0F;
        break;
    case StructureRotation::Clockwise180:
        facing = "north";
        yaw = 180.0F;
        break;
    case StructureRotation::Counterclockwise90:
        facing = "east";
        yaw = 270.0F;
        break;
    case StructureRotation::None:
    default:
        break;
    }
    const double x = position.x + (facing == "west" ? 0.96875 : facing == "east" ? 0.03125 : 0.5);
    const double z = position.z + (facing == "north" ? 0.96875 : facing == "south" ? 0.03125 : 0.5);
    wire::nbt::NbtCompound data;
    data.set("facing", wire::nbt::NbtTag::ofString(facing));
    data.set("item", wire::nbt::NbtTag::ofString("minecraft:elytra"));
    chunk.addGeneratedEntity(ChunkGeneratedEntity{.type = wire::common::Identifier::vanilla("item_frame"),
                                                  .x = x,
                                                  .y = position.y + 0.5,
                                                  .z = z,
                                                  .yaw = yaw,
                                                  .pitch = 0.0F,
                                                  .data = std::move(data)});
}

} // namespace basalt::server::world::terrain
