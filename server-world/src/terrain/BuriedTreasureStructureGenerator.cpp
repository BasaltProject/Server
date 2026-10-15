#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/terrain/BuriedTreasureStructureGenerator.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

/// The piece sits at offset 9 in both axes, which is why it never crosses a chunk edge.
constexpr std::int32_t kPieceOffset = 9;
constexpr std::string_view kDefaultFacing = "north";

/// parity: `Direction.values()`, down first and up second.
constexpr std::array<std::array<std::int32_t, 3>, 6> kNeighbours{
    {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}}};

constexpr std::array<std::string_view, 4> kHorizontal{"north", "east", "south", "west"};

[[nodiscard]] std::pair<std::int32_t, std::int32_t> offsetOf(std::string_view facing) {
    if (facing == "north") {
        return {0, -1};
    }
    if (facing == "east") {
        return {1, 0};
    }
    return facing == "south" ? std::pair{0, 1} : std::pair{-1, 0};
}

[[nodiscard]] std::string oppositeOf(std::string_view facing) {
    if (facing == "north") {
        return "south";
    }
    if (facing == "south") {
        return "north";
    }
    return facing == "east" ? "west" : "east";
}

[[nodiscard]] std::string clockwiseOf(std::string_view facing) {
    if (facing == "north") {
        return "east";
    }
    if (facing == "east") {
        return "south";
    }
    return facing == "south" ? "west" : "north";
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] bool isLiquid(std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    return path == "water" || path == "lava";
}

[[nodiscard]] std::int32_t sand() {
    static const std::int32_t stateId = [] {
        const registry::BlockState* found = registry::blocks::named("sand");
        return found == nullptr ? registry::blocks::air().stateId : found->stateId;
    }();
    return stateId;
}

const std::set<std::string, std::less<>>& floors() {
    static const std::set<std::string, std::less<>> paths = {"sandstone", "stone", "andesite", "granite", "diorite"};
    return paths;
}

/// parity: `StructurePiece.reorient`. One solid horizontal neighbour turns the chest away from it;
/// two or more and it falls through to walking the default facing around until it finds a side that
/// is not solid.
[[nodiscard]] std::string reorient(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::string solid;
    bool ambiguous = false;
    for (const std::string_view direction : kHorizontal) {
        const auto [dx, dz] = offsetOf(direction);
        const std::int32_t state = chunk.decorationBlockStateId(x + dx, y, z + dz);
        if (pathOf(state) == "chest") {
            return std::string{kDefaultFacing};
        }
        if (!registry::blockShapes::isSolidRender(state)) {
            continue;
        }
        if (!solid.empty()) {
            solid.clear();
            ambiguous = true;
            break;
        }
        solid = std::string{direction};
    }
    if (!ambiguous && !solid.empty()) {
        return oppositeOf(solid);
    }

    std::string facing{kDefaultFacing};
    for (std::int32_t step = 0; step < 3; ++step) {
        const auto [dx, dz] = offsetOf(facing);
        if (!registry::blockShapes::isSolidRender(chunk.decorationBlockStateId(x + dx, y, z + dz))) {
            return facing;
        }
        facing = step == 1 ? clockwiseOf(facing) : oppositeOf(facing);
    }
    return facing;
}

} // namespace

BuriedTreasureStructureGenerator::BuriedTreasureStructureGenerator(BiomeAt biomeAt,
                                                                   std::set<std::string, std::less<>> beachBiomes,
                                                                   SurfaceHeightAt surfaceHeightAt)
    : biomeAt_(std::move(biomeAt)), beachBiomes_(std::move(beachBiomes)), surfaceHeightAt_(std::move(surfaceHeightAt)) {
}

const structure::StructurePlacementGrid& BuriedTreasureStructureGenerator::grid() {
    static const structure::StructurePlacementGrid built{
        1, 0, 0, structure::SpreadType::Linear, 0.01F, structure::FrequencyReduction::LegacyType2};
    return built;
}

void BuriedTreasureStructureGenerator::shellAround(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z,
                                                   std::int32_t floorStateId, std::int32_t softStateId) {
    for (const std::array<std::int32_t, 3>& offset : kNeighbours) {
        const std::int32_t nx = x + offset[0];
        const std::int32_t ny = y + offset[1];
        const std::int32_t nz = z + offset[2];
        if (ny < kOverworldHeight.minY() || ny > kOverworldHeight.maxY()) {
            continue;
        }
        const std::int32_t state = chunk.decorationBlockStateId(nx, ny, nz);
        if (!registry::blocks::isAir(state) && !isLiquid(state)) {
            continue;
        }
        const std::int32_t underneath = chunk.decorationBlockStateId(nx, ny - 1, nz);
        const bool open = registry::blocks::isAir(underneath) || isLiquid(underneath);
        chunk.setDecorationBlockStateId(nx, ny, nz, open && offset[1] != 1 ? floorStateId : softStateId, std::nullopt,
                                        true);
    }
}

void BuriedTreasureStructureGenerator::placeChest(Chunk& chunk, std::int64_t seed, std::int32_t x, std::int32_t y,
                                                  std::int32_t z) {
    if (pathOf(chunk.blockStateId(x & 15, y, z & 15)) == "chest") {
        return;
    }
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kStructureIndex, kUndergroundStructuresStep);
    const std::map<std::string, std::string> properties = {
        {"facing", reorient(chunk, x, y, z)}, {"type", "single"}, {"waterlogged", "false"}};
    const std::optional<std::int32_t> stateId =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla("chest"), properties);
    if (!stateId.has_value()) {
        return;
    }
    chunk.setBlockStateId(x & 15, y, z & 15, *stateId);
    wire::nbt::NbtCompound data;
    data.set("LootTable", wire::nbt::NbtTag::ofString("minecraft:chests/buried_treasure"));
    data.set("LootTableSeed", wire::nbt::NbtTag::ofLong(random.nextLong()));
    data.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
    chunk.setBlockEntity(
        ChunkBlockEntity{x & 15, y, z & 15, wire::common::Identifier::vanilla("chest"), std::move(data)});
}

bool BuriedTreasureStructureGenerator::startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const {
    if (!grid().isStructureChunk(seed, chunkX, chunkZ)) {
        return false;
    }
    const std::int32_t centreX = chunkX * 16 + 8;
    const std::int32_t centreZ = chunkZ * 16 + 8;
    return beachBiomes_.contains(biomeAt_(centreX, surfaceHeightAt_(centreX, centreZ) - 1, centreZ));
}

void BuriedTreasureStructureGenerator::generateInto(Chunk& chunk, std::int64_t seed) {
    const std::int32_t chunkX = chunk.chunkX();
    const std::int32_t chunkZ = chunk.chunkZ();
    if (!grid().isStructureChunk(seed, chunkX, chunkZ)) {
        return;
    }
    // parity: `onTopOfChunkCenter` over `OCEAN_FLOOR_WG`, which is where the biome is tested.
    const std::int32_t centreX = chunkX * 16 + 8;
    const std::int32_t centreZ = chunkZ * 16 + 8;
    if (!beachBiomes_.contains(biomeAt_(centreX, surfaceHeightAt_(centreX, centreZ) - 1, centreZ))) {
        return;
    }
    bury(chunk, seed, chunkX, chunkZ);
}

std::optional<structure::StructureBoundingBox>
BuriedTreasureStructureGenerator::placeBoundsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const {
    static_cast<void>(seed);
    return structure::StructureBoundingBox{chunkX * 16,      kOverworldHeight.minY(), chunkZ * 16,
                                           chunkX * 16 + 15, kOverworldHeight.maxY(), chunkZ * 16 + 15};
}

void BuriedTreasureStructureGenerator::placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX,
                                               std::int32_t chunkZ) {
    bury(chunk, seed, chunkX, chunkZ, /*forPlacement=*/true);
}

void BuriedTreasureStructureGenerator::bury(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                                            bool forPlacement) {
    const std::int32_t x = chunkX * 16 + kPieceOffset;
    const std::int32_t z = chunkZ * 16 + kPieceOffset;
    for (std::int32_t y = chunk.heightAt(x & 15, z & 15, HeightmapType::OceanFloor); y > kOverworldHeight.minY(); --y) {
        const std::int32_t below = chunk.blockStateId(x & 15, y - 1, z & 15);
        if (!floors().contains(pathOf(below))) {
            continue;
        }
        const std::int32_t current = chunk.blockStateId(x & 15, y, z & 15);
        const std::int32_t soft = !registry::blocks::isAir(current) && !isLiquid(current) ? current : sand();
        shellAround(chunk, x, y, z, below, soft);
        placeChest(chunk, seed, x, y, z);
        if (forPlacement) {
            // parity: `PlaceCommand.placeStructure` writes the pieces but registers no start.
            return;
        }
        const structure::StructureBoundingBox box{x, y, z, x, y, z};
        chunk.structures().add(structure::StructureStartRecord{
            .structure = std::string{kName}, .chunkX = chunkX, .chunkZ = chunkZ, .fullBounds = box, .pieces = {box}});
        return;
    }
}

} // namespace basalt::server::world::terrain
