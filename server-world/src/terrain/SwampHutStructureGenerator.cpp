#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/SwampHutStructureGenerator.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

using structure::StructureBoundingBox;
using structure::StructureDirection;
using structure::StructureMirror;
using structure::StructurePos;
using structure::StructureRotation;
using structure::StructureTemplate;

constexpr std::string_view kBiome = "swamp";
constexpr std::int32_t kWidth = 7;
constexpr std::int32_t kHeight = 7;
constexpr std::int32_t kDepth = 9;
constexpr std::int32_t kFloorY = 64;

constexpr std::array<StructureDirection, 4> kHorizontal{StructureDirection::North, StructureDirection::East,
                                                        StructureDirection::South, StructureDirection::West};

[[nodiscard]] std::int32_t state(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

[[nodiscard]] std::int32_t stairs(std::string_view facing, std::string_view shape) {
    const std::map<std::string, std::string> properties = {
        {"facing", std::string{facing}}, {"half", "bottom"}, {"shape", std::string{shape}}, {"waterlogged", "false"}};
    return registry::blocks::stateIdOf(wire::common::Identifier::vanilla("spruce_stairs"), properties)
        .value_or(state("spruce_stairs"));
}

const std::set<std::string, std::less<>>& replaceable() {
    static const std::set<std::string, std::less<>> paths = {"water", "lava", "glow_lichen", "seagrass",
                                                             "tall_seagrass"};
    return paths;
}

/// parity: `StructurePiece.makeBoundingBox`, which swaps width and depth on the X axis.
[[nodiscard]] StructureBoundingBox makeBoundingBox(std::int32_t x, std::int32_t y, std::int32_t z,
                                                   StructureDirection orientation) {
    if (orientation == StructureDirection::North || orientation == StructureDirection::South) {
        return StructureBoundingBox{x, y, z, x + kWidth - 1, y + kHeight - 1, z + kDepth - 1};
    }
    return StructureBoundingBox{x, y, z, x + kDepth - 1, y + kHeight - 1, z + kWidth - 1};
}

/// parity: `ScatteredFeaturePiece` plus `SwampHutPiece`.
class Piece {
public:
    Piece(std::int32_t west, std::int32_t north, StructureDirection orientation)
        : orientation_(orientation), bounds_(makeBoundingBox(west, kFloorY, north, orientation)),
          mirror_(orientation == StructureDirection::South || orientation == StructureDirection::West
                      ? StructureMirror::LeftRight
                      : StructureMirror::None),
          rotation_(orientation == StructureDirection::West || orientation == StructureDirection::East
                        ? StructureRotation::Clockwise90
                        : StructureRotation::None) {}

    [[nodiscard]] const StructureBoundingBox& bounds() const noexcept { return bounds_; }

    /// parity: `updateAverageGroundHeight` averages every footprint column since the piece never leaves its chunk.
    bool settleOnGround(const Chunk& chunk);

    void place(Chunk& chunk) const;

private:
    void placeRoof(Chunk& chunk) const;
    /// parity: vanilla guards each spawn with a flag; a chunk here generates once.
    void spawnResidents(Chunk& chunk) const;

    void box(Chunk& chunk, std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
             std::int32_t z1, std::int32_t stateId) const;
    void set(Chunk& chunk, std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) const;
    void fillColumnDown(Chunk& chunk, std::int32_t stateId, std::int32_t x, std::int32_t startY, std::int32_t z) const;

    [[nodiscard]] std::int32_t transformed(std::int32_t stateId) const;
    [[nodiscard]] std::int32_t worldX(std::int32_t x, std::int32_t z) const;

    [[nodiscard]] std::int32_t worldY(std::int32_t y) const { return y + bounds_.minY; }

    [[nodiscard]] std::int32_t worldZ(std::int32_t x, std::int32_t z) const;

    StructureDirection orientation_;
    StructureBoundingBox bounds_;
    StructureMirror mirror_;
    StructureRotation rotation_;
};

bool Piece::settleOnGround(const Chunk& chunk) {
    std::int32_t total = 0;
    std::int32_t count = 0;
    for (std::int32_t z = bounds_.minZ; z <= bounds_.maxZ; ++z) {
        for (std::int32_t x = bounds_.minX; x <= bounds_.maxX; ++x) {
            total += chunk.heightAt(x & 15, z & 15, HeightmapType::MotionBlockingNoLeaves);
            ++count;
        }
    }
    if (count == 0) {
        return false;
    }
    bounds_ = bounds_.move(StructurePos{0, total / count - bounds_.minY, 0});
    return true;
}

std::int32_t Piece::worldX(std::int32_t x, std::int32_t z) const {
    switch (orientation_) {
    case StructureDirection::North:
    case StructureDirection::South:
        return bounds_.minX + x;
    case StructureDirection::West:
        return bounds_.maxX - z;
    case StructureDirection::East:
        return bounds_.minX + z;
    default:
        return x;
    }
}

std::int32_t Piece::worldZ(std::int32_t x, std::int32_t z) const {
    switch (orientation_) {
    case StructureDirection::North:
        return bounds_.maxZ - z;
    case StructureDirection::South:
        return bounds_.minZ + z;
    case StructureDirection::West:
    case StructureDirection::East:
        return bounds_.minZ + x;
    default:
        return z;
    }
}

std::int32_t Piece::transformed(std::int32_t stateId) const {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    if (properties.empty()) {
        return stateId;
    }
    return StructureTemplate::transformedStateId(stateId, properties, mirror_, rotation_);
}

void Piece::set(Chunk& chunk, std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) const {
    const std::int32_t targetY = worldY(y);
    if (targetY < kOverworldHeight.minY() || targetY > kOverworldHeight.maxY()) {
        return;
    }
    chunk.setBlockStateId(worldX(x, z) & 15, targetY, worldZ(x, z) & 15, transformed(stateId));
}

void Piece::box(Chunk& chunk, std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                std::int32_t z1, std::int32_t stateId) const {
    // The hut fills every box solid, so vanilla's edge/fill split collapses to one state.
    for (std::int32_t y = y0; y <= y1; ++y) {
        for (std::int32_t x = x0; x <= x1; ++x) {
            for (std::int32_t z = z0; z <= z1; ++z) {
                set(chunk, stateId, x, y, z);
            }
        }
    }
}

void Piece::fillColumnDown(Chunk& chunk, std::int32_t stateId, std::int32_t x, std::int32_t startY,
                           std::int32_t z) const {
    const std::int32_t localX = worldX(x, z) & 15;
    const std::int32_t localZ = worldZ(x, z) & 15;
    const auto isReplaceable = [](std::int32_t existing) {
        if (registry::blocks::isAir(existing)) {
            return true;
        }
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(existing);
        return block != nullptr && replaceable().contains(block->path());
    };
    for (std::int32_t y = worldY(startY);
         y > kOverworldHeight.minY() + 1 && isReplaceable(chunk.blockStateId(localX, y, localZ)); --y) {
        chunk.setBlockStateId(localX, y, localZ, stateId);
    }
}

void Piece::placeRoof(Chunk& chunk) const {
    box(chunk, 0, 4, 1, 6, 4, 1, stairs("north", "straight"));
    box(chunk, 0, 4, 2, 0, 4, 7, stairs("east", "straight"));
    box(chunk, 6, 4, 2, 6, 4, 7, stairs("west", "straight"));
    box(chunk, 0, 4, 8, 6, 4, 8, stairs("south", "straight"));
    set(chunk, stairs("north", "outer_right"), 0, 4, 1);
    set(chunk, stairs("north", "outer_left"), 6, 4, 1);
    set(chunk, stairs("south", "outer_left"), 0, 4, 8);
    set(chunk, stairs("south", "outer_right"), 6, 4, 8);
}

void Piece::spawnResidents(Chunk& chunk) const {
    const double x = worldX(2, 5) + 0.5;
    const double y = static_cast<double>(worldY(2));
    const double z = worldZ(2, 5) + 0.5;
    for (const std::string_view type : {"witch", "cat"}) {
        wire::nbt::NbtCompound data;
        data.set("PersistenceRequired", wire::nbt::NbtTag::ofByte(1));
        chunk.addGeneratedEntity(ChunkGeneratedEntity{.type = wire::common::Identifier::vanilla(std::string{type}),
                                                      .x = x,
                                                      .y = y,
                                                      .z = z,
                                                      .yaw = 0.0F,
                                                      .pitch = 0.0F,
                                                      .data = std::move(data)});
    }
}

void Piece::place(Chunk& chunk) const {
    const std::int32_t planks = state("spruce_planks");
    const std::int32_t log = state("oak_log");
    const std::int32_t fence = state("oak_fence");
    box(chunk, 1, 1, 1, 5, 1, 7, planks);
    box(chunk, 1, 4, 2, 5, 4, 7, planks);
    box(chunk, 2, 1, 0, 4, 1, 0, planks);
    box(chunk, 2, 2, 2, 3, 3, 2, planks);
    box(chunk, 1, 2, 3, 1, 3, 6, planks);
    box(chunk, 5, 2, 3, 5, 3, 6, planks);
    box(chunk, 2, 2, 7, 4, 3, 7, planks);
    box(chunk, 1, 0, 2, 1, 3, 2, log);
    box(chunk, 5, 0, 2, 5, 3, 2, log);
    box(chunk, 1, 0, 7, 1, 3, 7, log);
    box(chunk, 5, 0, 7, 5, 3, 7, log);
    set(chunk, fence, 2, 3, 2);
    set(chunk, fence, 3, 3, 7);
    set(chunk, registry::blocks::air().stateId, 1, 3, 4);
    set(chunk, registry::blocks::air().stateId, 5, 3, 4);
    set(chunk, registry::blocks::air().stateId, 5, 3, 5);
    set(chunk, state("potted_red_mushroom"), 1, 3, 5);
    set(chunk, state("crafting_table"), 3, 2, 6);
    set(chunk, state("cauldron"), 4, 2, 6);
    set(chunk, fence, 1, 2, 1);
    set(chunk, fence, 5, 2, 1);
    placeRoof(chunk);
    for (std::int32_t z = 2; z <= 7; z += 5) {
        for (std::int32_t x = 1; x <= 5; x += 4) {
            fillColumnDown(chunk, log, x, -1, z);
        }
    }
    spawnResidents(chunk);
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

} // namespace

SwampHutStructureGenerator::SwampHutStructureGenerator(BiomeAt biomeAt, SurfaceHeightAt surfaceHeightAt)
    : biomeAt_(std::move(biomeAt)), surfaceHeightAt_(std::move(surfaceHeightAt)) {}

const structure::StructurePlacementGrid& SwampHutStructureGenerator::grid() {
    static const structure::StructurePlacementGrid built{32, 8, 14357620};
    return built;
}

bool SwampHutStructureGenerator::startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const {
    if (!grid().isStructureChunk(seed, chunkX, chunkZ)) {
        return false;
    }
    // parity: `onTopOfChunkCenter` over `WORLD_SURFACE_WG`.
    const std::int32_t centreX = chunkX * 16 + 8;
    const std::int32_t centreZ = chunkZ * 16 + 8;
    const std::int32_t stubY = surfaceHeightAt_(centreX, centreZ) - 1;
    return biomePath(biomeAt_(centreX, stubY, centreZ)) == kBiome;
}

void SwampHutStructureGenerator::placeHut(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                                          bool forPlacement) {
    // parity: `Structure.GenerationContext`'s own random.
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, chunkX, chunkZ);

    const StructureDirection orientation =
        kHorizontal[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kHorizontal.size())))];
    Piece piece{chunkX * 16, chunkZ * 16, orientation};
    if (!piece.settleOnGround(chunk)) {
        return;
    }
    piece.place(chunk);
    if (forPlacement) {
        // parity: `PlaceCommand.placeStructure` writes the pieces but registers no start.
        return;
    }
    chunk.structures().add(structure::StructureStartRecord{.structure = std::string{kName},
                                                           .chunkX = chunkX,
                                                           .chunkZ = chunkZ,
                                                           .fullBounds = piece.bounds(),
                                                           .pieces = {piece.bounds()}});
}

void SwampHutStructureGenerator::generateInto(Chunk& chunk, std::int64_t seed) {
    const std::int32_t chunkX = chunk.chunkX();
    const std::int32_t chunkZ = chunk.chunkZ();
    if (!grid().isStructureChunk(seed, chunkX, chunkZ)) {
        return;
    }

    // parity: `onTopOfChunkCenter` over `WORLD_SURFACE_WG`.
    const std::int32_t centreX = chunkX * 16 + 8;
    const std::int32_t centreZ = chunkZ * 16 + 8;
    if (biomePath(biomeAt_(centreX, surfaceHeightAt_(centreX, centreZ) - 1, centreZ)) != kBiome) {
        return;
    }
    placeHut(chunk, seed, chunkX, chunkZ);
}

std::optional<structure::StructureBoundingBox>
SwampHutStructureGenerator::placeBoundsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const {
    static_cast<void>(seed);
    return structure::StructureBoundingBox{chunkX * 16,      kOverworldHeight.minY(), chunkZ * 16,
                                           chunkX * 16 + 15, kOverworldHeight.maxY(), chunkZ * 16 + 15};
}

void SwampHutStructureGenerator::placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) {
    placeHut(chunk, seed, chunkX, chunkZ, /*forPlacement=*/true);
}

} // namespace basalt::server::world::terrain
