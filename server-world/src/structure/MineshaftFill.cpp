#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/structure/MineshaftFill.hpp>
#include <basalt/server/world/structure/StructurePieceFill.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace basalt::server::world::structure {
namespace {

constexpr std::int32_t kLowerFloorHeight = 3;

[[nodiscard]] std::int32_t state(std::string_view path, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> found =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla(std::string{path}), properties);
    if (!found.has_value()) {
        throw std::runtime_error{"unknown state '" + std::string{path} + "'"};
    }
    return *found;
}

[[nodiscard]] std::int32_t named(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::int32_t cobweb() {
    static const std::int32_t stateId = named("cobweb");
    return stateId;
}

[[nodiscard]] std::int32_t lava() {
    static const std::int32_t stateId = named("lava");
    return stateId;
}

[[nodiscard]] std::int32_t ironChain() {
    static const std::int32_t stateId = named("iron_chain");
    return stateId;
}

[[nodiscard]] std::int32_t spawner() {
    static const std::int32_t stateId = named("spawner");
    return stateId;
}

[[nodiscard]] std::int32_t railNorthSouth() {
    static const std::int32_t stateId = state("rail", {{"shape", "north_south"}, {"waterlogged", "false"}});
    return stateId;
}

[[nodiscard]] std::int32_t railEastWest() {
    static const std::int32_t stateId = state("rail", {{"shape", "east_west"}, {"waterlogged", "false"}});
    return stateId;
}

[[nodiscard]] std::int32_t wallTorchSouth() {
    static const std::int32_t stateId = state("wall_torch", {{"facing", "south"}});
    return stateId;
}

[[nodiscard]] std::int32_t wallTorchNorth() {
    static const std::int32_t stateId = state("wall_torch", {{"facing", "north"}});
    return stateId;
}

/// parity: the `minecraft:mineshaft_blocking` biome tag.
const std::set<std::string, std::less<>>& blockingBiomes() {
    static const std::set<std::string, std::less<>> biomes = {"minecraft:deep_dark"};
    return biomes;
}

[[nodiscard]] const wire::common::Identifier* blockOf(std::int32_t stateId) {
    return registry::blockStateDefinitions::blockOf(stateId);
}

[[nodiscard]] bool sameBlock(std::int32_t first, std::int32_t second) {
    const wire::common::Identifier* left = blockOf(first);
    const wire::common::Identifier* right = blockOf(second);
    return left != nullptr && right != nullptr && *left == *right;
}

/// A piece never overwrites another piece's frame. parity: `MineShaftPiece.canBeReplaced`.
[[nodiscard]] bool canBeReplaced(std::int32_t stateId, const MineshaftBlocks& blocks) {
    const wire::common::Identifier* block = blockOf(stateId);
    if (block == nullptr) {
        return true;
    }
    return !sameBlock(stateId, blocks.planks()) && !sameBlock(stateId, blocks.log()) &&
           !sameBlock(stateId, blocks.fence()) && block->path() != "iron_chain";
}

/// parity: `MineShaftPiece.isInInvalidLocation`.
[[nodiscard]] bool isInInvalidLocation(const MineshaftPiece& piece, const Chunk& chunk,
                                       const StructureBoundingBox& clip, const mineshaftFill::BiomeAt& biomeAt) {
    const std::int32_t x0 = std::max(piece.box.minX - 1, clip.minX);
    const std::int32_t y0 = std::max(piece.box.minY - 1, clip.minY);
    const std::int32_t z0 = std::max(piece.box.minZ - 1, clip.minZ);
    const std::int32_t x1 = std::min(piece.box.maxX + 1, clip.maxX);
    const std::int32_t y1 = std::min(piece.box.maxY + 1, clip.maxY);
    const std::int32_t z1 = std::min(piece.box.maxZ + 1, clip.maxZ);

    // parity: the biome at the centre of the clipped box is tested before any block, and a shaft simply refuses to.
    if (biomeAt && blockingBiomes().contains(biomeAt((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))) {
        return true;
    }

    const auto liquid = [&chunk](std::int32_t x, std::int32_t y, std::int32_t z) {
        return registry::blockFluids::hasFluid(chunk.decorationBlockStateId(x, y, z));
    };
    for (std::int32_t x = x0; x <= x1; ++x) {
        for (std::int32_t z = z0; z <= z1; ++z) {
            if (liquid(x, y0, z) || liquid(x, y1, z)) {
                return true;
            }
        }
    }
    for (std::int32_t x = x0; x <= x1; ++x) {
        for (std::int32_t y = y0; y <= y1; ++y) {
            if (liquid(x, y, z0) || liquid(x, y, z1)) {
                return true;
            }
        }
    }
    for (std::int32_t z = z0; z <= z1; ++z) {
        for (std::int32_t y = y0; y <= y1; ++y) {
            if (liquid(x0, y, z) || liquid(x1, y, z)) {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] bool hasSturdyNeighbours(const StructurePieceFill& fill, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::int32_t sturdy = 0;
    static constexpr std::array<registry::BlockFace, registry::kBlockFaceCount> kFaces{
        registry::BlockFace::Down,  registry::BlockFace::Up,   registry::BlockFace::North,
        registry::BlockFace::South, registry::BlockFace::West, registry::BlockFace::East};
    for (const registry::BlockFace face : kFaces) {
        const std::int32_t neighbour =
            fill.blockAt(x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face));
        if (registry::blockShapes::isFaceSturdy(neighbour, registry::opposite(face))) {
            ++sturdy;
            if (sturdy >= 2) {
                return true;
            }
        }
    }
    return false;
}

/// parity: `maybePlaceCobWeb`.
void maybeCobweb(const StructurePieceFill& fill, random::RandomSource& random, float probability, std::int32_t x,
                 std::int32_t y, std::int32_t z) {
    if (!fill.isInterior(x, y, z)) {
        return;
    }
    if (random.nextFloat() >= probability) {
        return;
    }
    if (!hasSturdyNeighbours(fill, x, y, z)) {
        return;
    }
    fill.place(cobweb(), x, y, z);
}

/// parity: `setPlanksBlock`.
void setPlanks(const StructurePieceFill& fill, const MineshaftBlocks& blocks, std::int32_t x, std::int32_t y,
               std::int32_t z) {
    if (!fill.isInterior(x, y, z)) {
        return;
    }
    if (registry::blockShapes::isFaceSturdy(fill.blockAt(x, y, z), registry::BlockFace::Up)) {
        return;
    }
    fill.place(blocks.planks(), x, y, z);
}

/// parity: `StructurePiece.isReplaceableByStructures`.
[[nodiscard]] bool isReplaceableByStructures(std::int32_t stateId) {
    if (registry::blocks::isAir(stateId)) {
        return true;
    }
    static const std::set<std::string, std::less<>> paths = {"water",       "lava",     "bubble_column",
                                                             "glow_lichen", "seagrass", "tall_seagrass"};
    const wire::common::Identifier* block = blockOf(stateId);
    return block != nullptr && paths.find(block->path()) != paths.end();
}

/// parity: `Block.canSupportCenter(level, pos, Direction.DOWN)`.
[[nodiscard]] bool canSupportCentreBelow(std::int32_t stateId) {
    const wire::common::Identifier* block = blockOf(stateId);
    if (block != nullptr && block->path().ends_with("fence_gate")) {
        return false;
    }
    return registry::blockShapes::supportsCentre(stateId, registry::BlockFace::Down);
}

/// parity: `!(state.getBlock() instanceof FallingBlock)`.
[[nodiscard]] bool isFallingBlock(std::int32_t stateId) {
    static const std::set<std::string, std::less<>> paths = {"sand",  "red_sand",      "gravel",       "dragon_egg",
                                                             "anvil", "chipped_anvil", "damaged_anvil"};
    const wire::common::Identifier* block = blockOf(stateId);
    if (block == nullptr) {
        return false;
    }
    return paths.find(block->path()) != paths.end() || block->path().ends_with("_concrete_powder");
}

/// parity: `fillPillarDownOrChainUp`.
void fillPillarDownOrChainUp(const StructurePieceFill& fill, const MineshaftBlocks& blocks, std::int32_t x,
                             std::int32_t y, std::int32_t z) {
    const std::int32_t originY = fill.worldY(y);
    bool checkBelow = true;
    bool checkAbove = true;
    for (std::int32_t distance = 1; checkBelow || checkAbove; ++distance) {
        if (checkBelow) {
            const std::int32_t below = fill.blockAt(x, y - distance, z);
            const bool emptyBelow = isReplaceableByStructures(below) && !sameBlock(below, lava());
            if (!emptyBelow && registry::blockShapes::isFaceSturdy(below, registry::BlockFace::Up)) {
                for (std::int32_t pillarY = y - distance + 1; pillarY < y; ++pillarY) {
                    fill.placeRaw(blocks.log(), x, pillarY, z);
                }
                return;
            }
            checkBelow = distance <= 20 && emptyBelow && originY - distance > kOverworldHeight.minY() + 1;
        }
        if (checkAbove) {
            const std::int32_t above = fill.blockAt(x, y + distance, z);
            const bool emptyAbove = isReplaceableByStructures(above);
            if (!emptyAbove && canSupportCentreBelow(above) && !isFallingBlock(above)) {
                fill.placeRaw(blocks.fence(), x, y + 1, z);
                for (std::int32_t chainY = y + 2; chainY < y + distance; ++chainY) {
                    fill.placeRaw(ironChain(), x, chainY, z);
                }
                return;
            }
            checkAbove = distance <= 50 && emptyAbove && originY + distance < kOverworldHeight.maxY();
        }
    }
}

void placeDoubleSupport(const StructurePieceFill& fill, const MineshaftBlocks& blocks, std::int32_t x, std::int32_t y,
                        std::int32_t z) {
    if (sameBlock(fill.blockAt(x, y, z), blocks.planks())) {
        fillPillarDownOrChainUp(fill, blocks, x, y, z);
    }
    if (sameBlock(fill.blockAt(x + 2, y, z), blocks.planks())) {
        fillPillarDownOrChainUp(fill, blocks, x + 2, y, z);
    }
}

[[nodiscard]] bool isSupportingBox(const StructurePieceFill& fill, std::int32_t z) {
    for (std::int32_t x = 0; x <= 2; ++x) {
        if (registry::blocks::isAir(fill.blockAt(x, 3, z))) {
            return false;
        }
    }
    return true;
}

/// parity: `placeSupport`.
void placeSupport(const StructurePieceFill& fill, const MineshaftBlocks& blocks, random::RandomSource& random,
                  std::int32_t z) {
    if (!isSupportingBox(fill, z)) {
        return;
    }
    const std::int32_t caveAir = registry::blocks::caveAir().stateId;
    fill.generateBox(0, 0, z, 0, 1, z, blocks.fenceWest(), caveAir, false);
    fill.generateBox(2, 0, z, 2, 1, z, blocks.fenceEast(), caveAir, false);
    if (random.nextInt(4) == 0) {
        fill.generateBox(0, 2, z, 0, 2, z, blocks.planks(), caveAir, false);
        fill.generateBox(2, 2, z, 2, 2, z, blocks.planks(), caveAir, false);
        return;
    }
    fill.generateBox(0, 2, z, 2, 2, z, blocks.planks(), caveAir, false);
    fill.maybePlace(random, 0.05F, 1, 2, z - 1, wallTorchSouth());
    fill.maybePlace(random, 0.05F, 1, 2, z + 1, wallTorchNorth());
}

/// parity: `MineShaftCorridor.createChest`.
void placeChestRail(const StructurePieceFill& fill, random::RandomSource& random, std::int32_t x, std::int32_t y,
                    std::int32_t z) {
    if (!registry::blocks::isAir(fill.blockAt(x, y, z))) {
        return;
    }
    if (registry::blocks::isAir(fill.blockAt(x, y - 1, z))) {
        return;
    }
    fill.place(random.nextBoolean() ? railNorthSouth() : railEastWest(), x, y, z);
    const std::int64_t lootSeed = random.nextLong();
    wire::nbt::NbtCompound data;
    data.set("LootTable", wire::nbt::NbtTag::ofString("minecraft:chests/abandoned_mineshaft"));
    data.set("LootTableSeed", wire::nbt::NbtTag::ofLong(lootSeed));
    fill.placeEntity(wire::common::Identifier::vanilla("chest_minecart"), x, y, z, std::move(data));
}

/// parity: the one cave spider spawner a spider corridor gets.
void placeCaveSpiderSpawner(Chunk& chunk, const StructurePieceFill& fill, std::int32_t z) {
    const std::int32_t worldX = fill.worldX(1, z);
    const std::int32_t worldY = fill.worldY(0);
    const std::int32_t worldZ = fill.worldZ(1, z);
    chunk.setBlockStateId(worldX & 15, worldY, worldZ & 15, spawner());
    wire::nbt::NbtCompound entity;
    entity.set("id", wire::nbt::NbtTag::ofString("minecraft:mob_spawner"));
    entity.set("Delay", wire::nbt::NbtTag::ofShort(20));
    entity.set("MinSpawnDelay", wire::nbt::NbtTag::ofShort(200));
    entity.set("MaxSpawnDelay", wire::nbt::NbtTag::ofShort(800));
    entity.set("SpawnCount", wire::nbt::NbtTag::ofShort(4));
    entity.set("MaxNearbyEntities", wire::nbt::NbtTag::ofShort(6));
    entity.set("RequiredPlayerRange", wire::nbt::NbtTag::ofShort(16));
    entity.set("SpawnRange", wire::nbt::NbtTag::ofShort(4));
    wire::nbt::NbtCompound spawnEntity;
    spawnEntity.set("id", wire::nbt::NbtTag::ofString("minecraft:cave_spider"));
    wire::nbt::NbtCompound spawnData;
    spawnData.set("entity", wire::nbt::NbtTag::ofCompound(std::move(spawnEntity)));
    entity.set("SpawnData", wire::nbt::NbtTag::ofCompound(std::move(spawnData)));
    entity.set("SpawnPotentials", wire::nbt::NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::End}));
    entity.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
    chunk.setBlockEntity(ChunkBlockEntity{worldX & 15, worldY, worldZ & 15,
                                          wire::common::Identifier::vanilla("mob_spawner"), std::move(entity)});
}

void corridor(const MineshaftPiece& piece, const MineshaftBlocks& blocks, const StructurePieceFill& fill,
              random::RandomSource& random, Chunk& chunk) {
    const std::int32_t length = piece.sections * 5 - 1;
    const std::int32_t caveAir = registry::blocks::caveAir().stateId;
    fill.generateBox(0, 0, 0, 2, 1, length, caveAir, caveAir, false);
    fill.generateMaybeBox(random, 0.8F, 0, 2, 0, 2, 2, length, caveAir, caveAir, false, false);
    if (piece.spiderCorridor) {
        fill.generateMaybeBox(random, 0.6F, 0, 0, 0, 2, 1, length, cobweb(), caveAir, false, true);
    }

    bool placedSpider = false;
    for (std::int32_t section = 0; section < piece.sections; ++section) {
        const std::int32_t z = 2 + section * 5;
        placeSupport(fill, blocks, random, z);
        maybeCobweb(fill, random, 0.1F, 0, 2, z - 1);
        maybeCobweb(fill, random, 0.1F, 2, 2, z - 1);
        maybeCobweb(fill, random, 0.1F, 0, 2, z + 1);
        maybeCobweb(fill, random, 0.1F, 2, 2, z + 1);
        maybeCobweb(fill, random, 0.05F, 0, 2, z - 2);
        maybeCobweb(fill, random, 0.05F, 2, 2, z - 2);
        maybeCobweb(fill, random, 0.05F, 0, 2, z + 2);
        maybeCobweb(fill, random, 0.05F, 2, 2, z + 2);
        if (random.nextInt(100) == 0) {
            placeChestRail(fill, random, 2, 0, z - 1);
        }
        if (random.nextInt(100) == 0) {
            placeChestRail(fill, random, 0, 0, z + 1);
        }
        if (piece.spiderCorridor && !placedSpider) {
            const std::int32_t spawnerZ = z - 1 + random.nextInt(3);
            if (fill.isInterior(1, 0, spawnerZ)) {
                placedSpider = true;
                placeCaveSpiderSpawner(chunk, fill, spawnerZ);
            }
        }
    }

    for (std::int32_t x = 0; x <= 2; ++x) {
        for (std::int32_t z = 0; z <= length; ++z) {
            setPlanks(fill, blocks, x, -1, z);
        }
    }

    placeDoubleSupport(fill, blocks, 0, -1, 2);
    if (piece.sections > 1) {
        placeDoubleSupport(fill, blocks, 0, -1, length - 2);
    }

    if (!piece.hasRails) {
        return;
    }
    for (std::int32_t z = 0; z <= length; ++z) {
        const std::int32_t floor = fill.blockAt(1, -1, z);
        if (registry::blocks::isAir(floor) || !registry::blockShapes::isSolidRender(floor)) {
            continue;
        }
        fill.maybePlace(random, fill.isInterior(1, 0, z) ? 0.7F : 0.9F, 1, 0, z, railNorthSouth());
    }
}

void supportPillar(const StructurePieceFill& fill, const MineshaftBlocks& blocks, std::int32_t x, std::int32_t y0,
                   std::int32_t z, std::int32_t y1) {
    if (registry::blocks::isAir(fill.blockAt(x, y1 + 1, z))) {
        return;
    }
    fill.generateBox(x, y0, z, x, y1, z, blocks.planks(), registry::blocks::caveAir().stateId, false);
}

/// parity: `MineShaftCrossing.postProcess`, written in world coordinates like vanilla's.
void crossing(const MineshaftPiece& piece, const MineshaftBlocks& blocks, const StructurePieceFill& fill) {
    const StructureBoundingBox box = piece.box;
    const std::int32_t caveAir = registry::blocks::caveAir().stateId;
    const auto hollow = [&](std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                            std::int32_t z1) { fill.generateBox(x0, y0, z0, x1, y1, z1, caveAir, caveAir, false); };

    if (piece.twoFloored) {
        const std::int32_t lower = box.minY + kLowerFloorHeight - 1;
        hollow(box.minX + 1, box.minY, box.minZ, box.maxX - 1, lower, box.maxZ);
        hollow(box.minX, box.minY, box.minZ + 1, box.maxX, lower, box.maxZ - 1);
        hollow(box.minX + 1, box.maxY - 2, box.minZ, box.maxX - 1, box.maxY, box.maxZ);
        hollow(box.minX, box.maxY - 2, box.minZ + 1, box.maxX, box.maxY, box.maxZ - 1);
        const std::int32_t landing = box.minY + kLowerFloorHeight;
        hollow(box.minX + 1, landing, box.minZ + 1, box.maxX - 1, landing, box.maxZ - 1);
    } else {
        hollow(box.minX + 1, box.minY, box.minZ, box.maxX - 1, box.maxY, box.maxZ);
        hollow(box.minX, box.minY, box.minZ + 1, box.maxX, box.maxY, box.maxZ - 1);
    }

    supportPillar(fill, blocks, box.minX + 1, box.minY, box.minZ + 1, box.maxY);
    supportPillar(fill, blocks, box.minX + 1, box.minY, box.maxZ - 1, box.maxY);
    supportPillar(fill, blocks, box.maxX - 1, box.minY, box.minZ + 1, box.maxY);
    supportPillar(fill, blocks, box.maxX - 1, box.minY, box.maxZ - 1, box.maxY);
    for (std::int32_t x = box.minX; x <= box.maxX; ++x) {
        for (std::int32_t z = box.minZ; z <= box.maxZ; ++z) {
            setPlanks(fill, blocks, x, box.minY - 1, z);
        }
    }
}

/// parity: `MineShaftStairs.postProcess`, a landing at each end and five carved steps.
void stairs(const StructurePieceFill& fill) {
    const std::int32_t caveAir = registry::blocks::caveAir().stateId;
    fill.generateBox(0, 5, 0, 2, 7, 1, caveAir, caveAir, false);
    fill.generateBox(0, 0, 7, 2, 2, 8, caveAir, caveAir, false);
    for (std::int32_t step = 0; step < 5; ++step) {
        const std::int32_t bottom = 5 - step - (step < 4 ? 1 : 0);
        fill.generateBox(0, bottom, 2 + step, 2, 7 - step, 2 + step, caveAir, caveAir, false);
    }
}

/// parity: `MineShaftRoom.postProcess`: a hollow floor, a mouth per passage, and a dome.
void room(const MineshaftPiece& piece, const StructurePieceFill& fill) {
    const StructureBoundingBox box = piece.box;
    const std::int32_t caveAir = registry::blocks::caveAir().stateId;
    fill.generateBox(box.minX, box.minY + 1, box.minZ, box.maxX, std::min(box.minY + 3, box.maxY), box.maxZ, caveAir,
                     caveAir, false);
    for (const StructureBoundingBox& entrance : piece.entrances) {
        fill.generateBox(entrance.minX, entrance.maxY - 2, entrance.minZ, entrance.maxX, entrance.maxY, entrance.maxZ,
                         caveAir, caveAir, false);
    }
    fill.generateUpperHalfSphere(box.minX, box.minY + 4, box.minZ, box.maxX, box.maxY, box.maxZ, caveAir, false);
}

} // namespace

void mineshaftFill::place(const MineshaftPiece& piece, const MineshaftBlocks& blocks, Chunk& chunk,
                          StructureBoundingBox clip, random::RandomSource& random, const BiomeAt& biomeAt) {
    const StructurePieceFill fill{piece.box, piece.fillOrientation(), chunk, clip,
                                  [&blocks](std::int32_t stateId) { return canBeReplaced(stateId, blocks); }};
    if (isInInvalidLocation(piece, chunk, clip, biomeAt)) {
        return;
    }
    switch (piece.kind) {
    case MineshaftPiece::Kind::Corridor:
        corridor(piece, blocks, fill, random, chunk);
        return;
    case MineshaftPiece::Kind::Crossing:
        crossing(piece, blocks, fill);
        return;
    case MineshaftPiece::Kind::Stairs:
        stairs(fill);
        return;
    case MineshaftPiece::Kind::Room:
        room(piece, fill);
        return;
    }
}

} // namespace basalt::server::world::structure
