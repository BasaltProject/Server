#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/structure/TemplateBlockProcessors.hpp>
#include <basalt/server/world/terrain/ShipwreckStructureGenerator.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain {
namespace {

using structure::StructureBoundingBox;
using structure::StructureMirror;
using structure::StructurePos;
using structure::StructureRotation;

/// parity: `ShipwreckPieces.PIVOT`.
constexpr StructurePos kPivot{4, 0, 15};

/// A hull is at most 32 blocks in x or z, so it never reaches further than two chunks.
constexpr std::int32_t kReachChunks = 3;

constexpr std::array<StructureRotation, 4> kRotations{StructureRotation::None, StructureRotation::Clockwise90,
                                                      StructureRotation::Clockwise180,
                                                      StructureRotation::Counterclockwise90};

constexpr std::array<std::string_view, 11> kBeachedHulls{"shipwreck/with_mast",
                                                         "shipwreck/sideways_full",
                                                         "shipwreck/sideways_fronthalf",
                                                         "shipwreck/sideways_backhalf",
                                                         "shipwreck/rightsideup_full",
                                                         "shipwreck/rightsideup_fronthalf",
                                                         "shipwreck/rightsideup_backhalf",
                                                         "shipwreck/with_mast_degraded",
                                                         "shipwreck/rightsideup_full_degraded",
                                                         "shipwreck/rightsideup_fronthalf_degraded",
                                                         "shipwreck/rightsideup_backhalf_degraded"};

constexpr std::array<std::string_view, 20> kOceanHulls{"shipwreck/with_mast",
                                                       "shipwreck/upsidedown_full",
                                                       "shipwreck/upsidedown_fronthalf",
                                                       "shipwreck/upsidedown_backhalf",
                                                       "shipwreck/sideways_full",
                                                       "shipwreck/sideways_fronthalf",
                                                       "shipwreck/sideways_backhalf",
                                                       "shipwreck/rightsideup_full",
                                                       "shipwreck/rightsideup_fronthalf",
                                                       "shipwreck/rightsideup_backhalf",
                                                       "shipwreck/with_mast_degraded",
                                                       "shipwreck/upsidedown_full_degraded",
                                                       "shipwreck/upsidedown_fronthalf_degraded",
                                                       "shipwreck/upsidedown_backhalf_degraded",
                                                       "shipwreck/sideways_full_degraded",
                                                       "shipwreck/sideways_fronthalf_degraded",
                                                       "shipwreck/sideways_backhalf_degraded",
                                                       "shipwreck/rightsideup_full_degraded",
                                                       "shipwreck/rightsideup_fronthalf_degraded",
                                                       "shipwreck/rightsideup_backhalf_degraded"};

[[nodiscard]] std::string_view markerLoot(std::string_view metadata) {
    if (metadata == "map_chest") {
        return "minecraft:chests/shipwreck_map";
    }
    if (metadata == "treasure_chest") {
        return "minecraft:chests/shipwreck_treasure";
    }
    return metadata == "supply_chest" ? "minecraft:chests/shipwreck_supply" : std::string_view{};
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::int32_t floorDiv(std::int32_t value, std::int32_t divisor) {
    const std::int32_t quotient = value / divisor;
    return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
}

} // namespace

ShipwreckStructureGenerator::ShipwreckStructureGenerator(BiomeAt biomeAt,
                                                         std::set<std::string, std::less<>> oceanBiomes,
                                                         std::set<std::string, std::less<>> beachBiomes,
                                                         TemplateOf templateOf, HeightAt surfaceHeightAt,
                                                         HeightAt oceanFloorAt, WorldHeightAt worldHeightAt)
    : biomeAt_(std::move(biomeAt)), oceanBiomes_(std::move(oceanBiomes)), beachBiomes_(std::move(beachBiomes)),
      templateOf_(std::move(templateOf)), surfaceHeightAt_(std::move(surfaceHeightAt)),
      oceanFloorAt_(std::move(oceanFloorAt)), worldHeightAt_(std::move(worldHeightAt)) {}

const structure::StructurePlacementGrid& ShipwreckStructureGenerator::grid() {
    static const structure::StructurePlacementGrid built{24, 4, 165745295};
    return built;
}

bool ShipwreckStructureGenerator::startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                                           bool beached) const {
    if (!grid().isStructureChunk(seed, chunkX, chunkZ)) {
        return false;
    }
    const std::optional<bool> chosen = chooseVariant(seed, chunkX, chunkZ);
    return chosen.has_value() && *chosen == beached;
}

std::optional<bool> ShipwreckStructureGenerator::chooseVariant(std::int64_t seed, std::int32_t chunkX,
                                                               std::int32_t chunkZ) const {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, chunkX, chunkZ);
    const std::int32_t centreX = chunkX * 16 + 8;
    const std::int32_t centreZ = chunkZ * 16 + 8;
    // false is `shipwreck`, true is `shipwreck_beached`, in the set's own order.
    std::vector<bool> options{false, true};
    while (!options.empty()) {
        const std::size_t index = static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(options.size())));
        const bool chosen = options[index];
        const bool fits =
            chosen ? beachBiomes_.contains(biomeAt_(centreX, surfaceHeightAt_(centreX, centreZ) - 1, centreZ))
                   : oceanBiomes_.contains(biomeAt_(centreX, oceanFloorAt_(centreX, centreZ) - 1, centreZ));
        if (fits) {
            return chosen;
        }
        options.erase(options.begin() + static_cast<std::ptrdiff_t>(index));
    }
    return std::nullopt;
}

std::optional<StructurePos> ShipwreckStructureGenerator::settle(const structure::StructureTemplate& piece,
                                                                StructurePos origin, bool beached,
                                                                random::RandomSource& random) const {
    const StructurePos size = piece.size();
    const std::int32_t footprint = size.x * size.z;
    if (footprint == 0) {
        return std::nullopt;
    }
    std::int32_t total = 0;
    std::int32_t lowest = kOverworldHeight.maxY() + 1;
    for (std::int32_t dx = 0; dx < size.x; ++dx) {
        for (std::int32_t dz = 0; dz < size.z; ++dz) {
            // parity: `ShipwreckPiece.postProcess` averages `level.getHeight` over the footprint,
            // not the density estimate the placement used to pick the spot.
            const std::int32_t height = worldHeightAt_(
                origin.x + dx, origin.z + dz, beached ? HeightmapType::WorldSurface : HeightmapType::OceanFloor);
            total += height;
            lowest = std::min(lowest, height);
        }
    }
    const std::int32_t y = beached ? lowest - size.y / 2 - random.nextInt(3) : total / footprint;
    return StructurePos{origin.x, y, origin.z};
}

void ShipwreckStructureGenerator::placeMarkerChests(Chunk& chunk, const structure::StructureTemplate& piece,
                                                    StructurePos position, StructureRotation rotation,
                                                    const StructureBoundingBox& clip, random::RandomSource& random) {
    for (const structure::StructureDataMarker& marker :
         piece.dataMarkers(position, rotation, StructureMirror::None, kPivot)) {
        const std::string_view lootTable = markerLoot(marker.metadata);
        if (lootTable.empty()) {
            continue;
        }
        const StructurePos below{marker.pos.x, marker.pos.y - 1, marker.pos.z};
        if (!clip.isInside(below)) {
            continue;
        }
        const std::string path = pathOf(chunk.blockStateId(below.x & 15, below.y, below.z & 15));
        if (path != "chest" && path != "barrel") {
            continue;
        }
        wire::nbt::NbtCompound data;
        data.set("LootTable", wire::nbt::NbtTag::ofString(std::string{lootTable}));
        data.set("LootTableSeed", wire::nbt::NbtTag::ofLong(random.nextLong()));
        data.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
        chunk.setBlockEntity(ChunkBlockEntity{below.x & 15, below.y, below.z & 15,
                                              wire::common::Identifier::vanilla(path), std::move(data)});
    }
}

std::optional<ShipwreckStructureGenerator::HullStart> ShipwreckStructureGenerator::hullStart(std::int64_t seed,
                                                                                             std::int32_t chunkX,
                                                                                             std::int32_t chunkZ,
                                                                                             bool beached) const {
    // parity: `context.random()`, a separate stream with the same seeding, so the rotation is its
    // first draw whichever variant was asked for.
    random::RandomSource pick = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(pick, seed, chunkX, chunkZ);
    const StructureRotation rotation =
        kRotations[static_cast<std::size_t>(pick.nextInt(static_cast<std::int32_t>(kRotations.size())))];
    const std::span<const std::string_view> names =
        beached ? std::span<const std::string_view>{kBeachedHulls} : std::span<const std::string_view>{kOceanHulls};
    const structure::StructureTemplate* piece =
        templateOf_(names[static_cast<std::size_t>(pick.nextInt(static_cast<std::int32_t>(names.size())))]);
    if (piece == nullptr) {
        return std::nullopt;
    }
    const StructurePos origin{chunkX * 16, 0, chunkZ * 16};
    const std::optional<StructurePos> settled = settle(*piece, origin, beached, pick);
    if (!settled.has_value()) {
        return std::nullopt;
    }
    return HullStart{.piece = piece, .rotation = rotation, .settled = *settled};
}

std::optional<structure::StructureBoundingBox> ShipwreckStructureGenerator::placeBoundsAt(std::int64_t seed,
                                                                                          std::int32_t chunkX,
                                                                                          std::int32_t chunkZ,
                                                                                          bool beached) const {
    const std::optional<HullStart> start = hullStart(seed, chunkX, chunkZ, beached);
    return start.has_value() ? std::optional{start->piece->getBoundingBox(start->settled, start->rotation, kPivot,
                                                                          StructureMirror::None)}
                             : std::nullopt;
}

void ShipwreckStructureGenerator::placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                                          bool beached) const {
    const std::optional<HullStart> start = hullStart(seed, chunkX, chunkZ, beached);
    if (!start.has_value()) {
        return;
    }
    const StructureBoundingBox clip{chunk.chunkX() * 16,      kOverworldHeight.minY(), chunk.chunkZ() * 16,
                                    chunk.chunkX() * 16 + 15, kOverworldHeight.maxY(), chunk.chunkZ() * 16 + 15};
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, beached ? kBeachedStructureIndex : kSunkStructureIndex,
                           kSurfaceStructuresStep);
    start->piece->placeInto(chunk, structure::TemplatePlacement{
                                       .position = start->settled,
                                       .rotation = start->rotation,
                                       .mirror = StructureMirror::None,
                                       .pivot = kPivot,
                                       .clipTo = clip,
                                       .blockProcessor = structure::templateBlockProcessors::ignoreStructureAndAir(),
                                       .random = &random});
    placeMarkerChests(chunk, *start->piece, start->settled, start->rotation, clip, random);
    // parity: `PlaceCommand.placeStructure` writes the pieces but registers no start.
}

void ShipwreckStructureGenerator::placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX,
                                            std::int32_t chunkZ) const {
    if (!grid().isStructureChunk(seed, chunkX, chunkZ)) {
        return;
    }

    // parity: a set with more than one structure *draws* which one, with its own
    // `setLargeFeatureSeed` random, and only then tests that one's biomes; a rejected option is
    // removed and the draw repeats. Deciding by biome instead looks equivalent and is not: the two
    // variants read different hull lists, so the draw that follows lands on another ship.
    const std::optional<bool> beached = chooseVariant(seed, chunkX, chunkZ);
    if (!beached.has_value()) {
        return;
    }

    // parity: `context.random()`, a separate stream with the same seeding, so the rotation is its
    // first draw whichever variant won.
    random::RandomSource pick = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(pick, seed, chunkX, chunkZ);
    const StructureRotation rotation =
        kRotations[static_cast<std::size_t>(pick.nextInt(static_cast<std::int32_t>(kRotations.size())))];
    const std::span<const std::string_view> names =
        *beached ? std::span<const std::string_view>{kBeachedHulls} : std::span<const std::string_view>{kOceanHulls};
    const structure::StructureTemplate* piece =
        templateOf_(names[static_cast<std::size_t>(pick.nextInt(static_cast<std::int32_t>(names.size())))]);
    if (piece == nullptr) {
        return;
    }

    const StructurePos origin{chunkX * 16, 0, chunkZ * 16};
    const std::optional<StructurePos> settled = settle(*piece, origin, *beached, pick);
    if (!settled.has_value()) {
        return;
    }
    const StructureBoundingBox clip{chunk.chunkX() * 16,      kOverworldHeight.minY(), chunk.chunkZ() * 16,
                                    chunk.chunkX() * 16 + 15, kOverworldHeight.maxY(), chunk.chunkZ() * 16 + 15};
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, *beached ? kBeachedStructureIndex : kSunkStructureIndex,
                           kSurfaceStructuresStep);
    piece->placeInto(chunk, structure::TemplatePlacement{
                                .position = *settled,
                                .rotation = rotation,
                                .mirror = StructureMirror::None,
                                .pivot = kPivot,
                                .clipTo = clip,
                                .blockProcessor = structure::templateBlockProcessors::ignoreStructureAndAir(),
                                .random = &random});
    placeMarkerChests(chunk, *piece, *settled, rotation, clip, random);
    const StructureBoundingBox box = piece->getBoundingBox(*settled, rotation, kPivot, StructureMirror::None);
    if (box.intersects(clip)) {
        chunk.structures().add(structure::StructureStartRecord{
            .structure = std::string{kName}, .chunkX = chunkX, .chunkZ = chunkZ, .fullBounds = box, .pieces = {box}});
    }
}

void ShipwreckStructureGenerator::generateInto(Chunk& chunk, std::int64_t seed) {
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
