#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/CaveVineColumnFeature.hpp>
#include <basalt/server/world/terrain/decoration/LushVegetationPatchFeature.hpp>
#include <basalt/server/world/terrain/decoration/RootedAzaleaFeature.hpp>
#include <basalt/server/world/terrain/decoration/StandaloneFeatures.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kPlatformRadius = 16;
constexpr std::int32_t kPlatformX = 8;
constexpr std::int32_t kPlatformZ = 8;
constexpr std::int32_t kPlatformYOffset = 3;
constexpr std::int32_t kPlatformChunkX = 0;
constexpr std::int32_t kPlatformChunkZ = 0;
constexpr std::int32_t kLakeWidth = 16;
constexpr std::int32_t kLakeHeight = 8;
constexpr std::int32_t kLakeFluidHeight = 4;
constexpr std::int32_t kLakeGridSize = kLakeWidth * kLakeWidth * kLakeHeight;
constexpr std::int32_t kSulfurVentScanSteps = 4;

struct HorizontalDirection {
    std::string_view name;
    std::int32_t dx;
    std::int32_t dz;
    std::string_view opposite;
    std::string_view clockwise;
};

constexpr std::array<HorizontalDirection, 4> kHorizontalDirections{
    HorizontalDirection{"north", 0, -1, "south", "east"}, HorizontalDirection{"east", 1, 0, "west", "south"},
    HorizontalDirection{"south", 0, 1, "north", "west"}, HorizontalDirection{"west", -1, 0, "east", "north"}};

constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> kHorizontalOffsets{std::pair{0, -1}, std::pair{0, 1},
                                                                                  std::pair{-1, 0}, std::pair{1, 0}};

struct VineFace {
    std::string_view property;
    std::int32_t dx;
    std::int32_t dy;
    std::int32_t dz;
    registry::BlockFace supportFace;
};

constexpr std::array<VineFace, 5> kVineFaces{
    VineFace{"up", 0, 1, 0, registry::BlockFace::Down}, VineFace{"north", 0, 0, -1, registry::BlockFace::South},
    VineFace{"south", 0, 0, 1, registry::BlockFace::North}, VineFace{"west", -1, 0, 0, registry::BlockFace::East},
    VineFace{"east", 1, 0, 0, registry::BlockFace::West}};

constexpr std::array<std::string_view, 4> kDungeonMobs{"minecraft:skeleton", "minecraft:zombie", "minecraft:zombie",
                                                       "minecraft:spider"};

using PathSet = std::set<std::string, std::less<>>;

const PathSet& featuresCannotReplace() {
    static const PathSet paths = {"bedrock",       "spawner", "chest", "end_portal_frame", "reinforced_deepslate",
                                  "trial_spawner", "vault"};
    return paths;
}

const PathSet& liquidBlockPaths() {
    static const PathSet paths = {"water", "lava", "bubble_column"};
    return paths;
}

[[nodiscard]] std::int32_t state(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

[[nodiscard]] std::int32_t state(std::string_view path, std::map<std::string, std::string> properties) {
    const std::optional<std::int32_t> resolved =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla(std::string{path}), properties);
    if (!resolved.has_value()) {
        throw std::runtime_error{"unknown state '" + std::string{path} + "'"};
    }
    return *resolved;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

/// These two read and write in world coordinates through the chunk's own decoration view. A lake is
/// 16 wide and a monster room 11, so both all but always cross an edge: reading outside as air made
/// a lake reject itself. A write outside is deferred, guarded by the very block it replaces.
[[nodiscard]] std::int32_t stateAt(const Chunk& chunk, const DecorationPos& position) {
    return chunk.decorationBlockStateId(position.x, position.y, position.z);
}

void set(Chunk& chunk, const DecorationPos& position, std::int32_t stateId) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (position.y < bounds.minY() || position.y > bounds.maxY()) {
        return;
    }
    chunk.setDecorationBlockStateId(position.x, position.y, position.z, stateId,
                                    chunk.decorationBlockStateId(position.x, position.y, position.z));
}

[[nodiscard]] DecorationPos offset(const DecorationPos& position, std::int32_t dx, std::int32_t dy, std::int32_t dz) {
    return DecorationPos{.x = position.x + dx, .y = position.y + dy, .z = position.z + dz};
}

[[nodiscard]] bool local(const Chunk& chunk, const DecorationPos& position) {
    return (position.x >> 4) == chunk.chunkX() && (position.z >> 4) == chunk.chunkZ();
}

[[nodiscard]] bool safeSetFeature(Chunk& chunk, const DecorationPos& position, std::int32_t stateId) {
    if (featuresCannotReplace().contains(pathOf(stateAt(chunk, position)))) {
        return false;
    }
    set(chunk, position, stateId);
    return local(chunk, position);
}

[[nodiscard]] std::int32_t checkerboardDistance(std::int32_t x1, std::int32_t z1, std::int32_t x2, std::int32_t z2) {
    return std::max(std::abs(x1 - x2), std::abs(z1 - z2));
}

/// parity: `SpringFeature`, which counts the rocks and the holes around the origin before writing.
bool placeSpring(Chunk& chunk, const DecorationPos& origin, std::string_view validBlock, std::int32_t fluidStateId,
                 std::int32_t rockCount, std::int32_t holeCount) {
    if (pathOf(stateAt(chunk, offset(origin, 0, 1, 0))) != validBlock) {
        return false;
    }
    const std::int32_t current = stateAt(chunk, origin);
    if (!registry::blocks::isAir(current) && pathOf(current) != validBlock) {
        return false;
    }
    const std::array<DecorationPos, 5> sides{offset(origin, -1, 0, 0), offset(origin, 1, 0, 0),
                                             offset(origin, 0, 0, -1), offset(origin, 0, 0, 1),
                                             offset(origin, 0, -1, 0)};
    std::int32_t rocks = 0;
    std::int32_t holes = 0;
    for (const DecorationPos& side : sides) {
        if (pathOf(stateAt(chunk, side)) == validBlock) {
            ++rocks;
        }
        if (registry::blocks::isAir(stateAt(chunk, side))) {
            ++holes;
        }
    }
    if (rocks != rockCount || holes != holeCount) {
        return false;
    }
    set(chunk, origin, fluidStateId);
    const wire::common::Identifier* fluid = registry::blockStateDefinitions::blockOf(fluidStateId);
    if (fluid != nullptr && local(chunk, origin)) {
        chunk.scheduleFluidTick(ChunkScheduledTick{
            .type = *fluid, .localX = origin.x & 15, .y = origin.y, .localZ = origin.z & 15, .delay = 0});
    }
    return true;
}

[[nodiscard]] bool isSolidRenderTowards(const Chunk& chunk, const DecorationPos& position,
                                        const HorizontalDirection& direction) {
    return registry::blockShapes::isSolidRender(stateAt(chunk, offset(position, direction.dx, 0, direction.dz)));
}

[[nodiscard]] const HorizontalDirection& directionNamed(std::string_view name) {
    for (const HorizontalDirection& direction : kHorizontalDirections) {
        if (direction.name == name) {
            return direction;
        }
    }
    throw std::logic_error{"unknown horizontal direction"};
}

/// parity: `StructurePiece.reorient`, including its adjacent-chest and multi-wall fallbacks.
[[nodiscard]] std::string reorientDungeonChest(const Chunk& chunk, const DecorationPos& position) {
    const HorizontalDirection* solidDirection = nullptr;
    for (const HorizontalDirection& direction : kHorizontalDirections) {
        const std::int32_t neighbour = stateAt(chunk, offset(position, direction.dx, 0, direction.dz));
        if (pathOf(neighbour) == "chest") {
            return "north";
        }
        if (!registry::blockShapes::isSolidRender(neighbour)) {
            continue;
        }
        if (solidDirection != nullptr) {
            solidDirection = nullptr;
            break;
        }
        solidDirection = &direction;
    }
    if (solidDirection != nullptr) {
        return std::string{solidDirection->opposite};
    }

    const HorizontalDirection* facing = &kHorizontalDirections.front();
    if (isSolidRenderTowards(chunk, position, *facing)) {
        facing = &directionNamed(facing->opposite);
    }
    if (isSolidRenderTowards(chunk, position, *facing)) {
        facing = &directionNamed(facing->clockwise);
    }
    if (isSolidRenderTowards(chunk, position, *facing)) {
        facing = &directionNamed(facing->opposite);
    }
    return std::string{facing->name};
}

bool placeDungeonChest(Chunk& chunk, random::RandomSource& random, const DecorationPos& position) {
    if (!local(chunk, position)) {
        return false;
    }
    const std::int32_t chestStateId = state(
        "chest", {{"facing", reorientDungeonChest(chunk, position)}, {"type", "single"}, {"waterlogged", "false"}});
    if (!safeSetFeature(chunk, position, chestStateId)) {
        return false;
    }
    wire::nbt::NbtCompound data;
    data.putString("id", "minecraft:chest");
    data.putString("LootTable", "minecraft:chests/simple_dungeon");
    data.putLong("LootTableSeed", random.nextLong());
    data.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
    chunk.setBlockEntity(ChunkBlockEntity{position.x & 15, position.y, position.z & 15,
                                          wire::common::Identifier::vanilla("chest"), std::move(data)});
    return true;
}

void placeDungeonSpawner(Chunk& chunk, random::RandomSource& random, const DecorationPos& position) {
    if (!local(chunk, position)) {
        return;
    }
    const std::string_view mob = kDungeonMobs[static_cast<std::size_t>(random.nextInt(kDungeonMobs.size()))];
    wire::nbt::NbtCompound entity;
    entity.putString("id", std::string{mob});
    wire::nbt::NbtCompound spawnData;
    spawnData.set("entity", wire::nbt::NbtTag::ofCompound(std::move(entity)));
    wire::nbt::NbtCompound data;
    data.putString("id", "minecraft:mob_spawner");
    data.putShort("Delay", 20);
    data.putShort("MinSpawnDelay", 200);
    data.putShort("MaxSpawnDelay", 800);
    data.putShort("SpawnCount", 4);
    data.putShort("MaxNearbyEntities", 6);
    data.putShort("RequiredPlayerRange", 16);
    data.putShort("SpawnRange", 4);
    data.set("SpawnData", wire::nbt::NbtTag::ofCompound(std::move(spawnData)));
    data.set("SpawnPotentials", wire::nbt::NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::End}));
    data.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
    chunk.setBlockEntity(ChunkBlockEntity{position.x & 15, position.y, position.z & 15,
                                          wire::common::Identifier::vanilla("mob_spawner"), std::move(data)});
}

/// parity: `MonsterRoomFeature`: protected carve, up to two loot chests, and a populated spawner.
bool placeMonsterRoom(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const std::int32_t radiusX = random.nextInt(2) + 2;
    const std::int32_t radiusZ = random.nextInt(2) + 2;
    const std::int32_t minX = -radiusX - 1;
    const std::int32_t maxX = radiusX + 1;
    const std::int32_t minZ = -radiusZ - 1;
    const std::int32_t maxZ = radiusZ + 1;

    std::int32_t openings = 0;
    for (std::int32_t dx = minX; dx <= maxX; ++dx) {
        for (std::int32_t dy = -1; dy <= 4; ++dy) {
            for (std::int32_t dz = minZ; dz <= maxZ; ++dz) {
                const DecorationPos position = offset(origin, dx, dy, dz);
                const bool solid = registry::blockShapes::isSolidState(stateAt(chunk, position));
                if ((dy == -1 || dy == 4) && !solid) {
                    return false;
                }
                const bool onWall = dx == minX || dx == maxX || dz == minZ || dz == maxZ;
                if (onWall && dy == 0 && registry::blocks::isAir(stateAt(chunk, position)) &&
                    registry::blocks::isAir(stateAt(chunk, offset(position, 0, 1, 0)))) {
                    ++openings;
                }
            }
        }
    }
    if (openings < 1 || openings > 5) {
        return false;
    }

    for (std::int32_t dx = minX; dx <= maxX; ++dx) {
        for (std::int32_t dy = 3; dy >= -1; --dy) {
            for (std::int32_t dz = minZ; dz <= maxZ; ++dz) {
                const DecorationPos position = offset(origin, dx, dy, dz);
                const bool onShell = dx == minX || dy == -1 || dz == minZ || dx == maxX || dy == 4 || dz == maxZ;
                if (!onShell) {
                    const std::string path = pathOf(stateAt(chunk, position));
                    if (path != "chest" && path != "spawner") {
                        static_cast<void>(safeSetFeature(chunk, position, state("cave_air")));
                    }
                    continue;
                }
                const DecorationPos below = offset(position, 0, -1, 0);
                const std::int32_t wallStateId = stateAt(chunk, position);
                if (position.y >= chunk.verticalBounds().minY() &&
                    !registry::blockShapes::isSolidState(stateAt(chunk, below))) {
                    set(chunk, position, state("cave_air"));
                } else if (registry::blockShapes::isSolidState(wallStateId) && pathOf(wallStateId) != "chest") {
                    const bool mossy = dy == -1 && random.nextInt(4) != 0;
                    static_cast<void>(
                        safeSetFeature(chunk, position, mossy ? state("mossy_cobblestone") : state("cobblestone")));
                }
            }
        }
    }

    for (std::int32_t chest = 0; chest < 2; ++chest) {
        for (std::int32_t attempt = 0; attempt < 3; ++attempt) {
            const DecorationPos chestPosition{.x = origin.x + random.nextInt(radiusX * 2 + 1) - radiusX,
                                              .y = origin.y,
                                              .z = origin.z + random.nextInt(radiusZ * 2 + 1) - radiusZ};
            if (!registry::blocks::isAir(stateAt(chunk, chestPosition))) {
                continue;
            }
            std::int32_t solidNeighbours = 0;
            for (const auto& [dx, dz] : kHorizontalOffsets) {
                if (registry::blockShapes::isSolidState(stateAt(chunk, offset(chestPosition, dx, 0, dz)))) {
                    ++solidNeighbours;
                }
            }
            if (solidNeighbours == 1 && placeDungeonChest(chunk, random, chestPosition)) {
                break;
            }
        }
    }

    static_cast<void>(safeSetFeature(chunk, origin, state("spawner")));
    if (pathOf(stateAt(chunk, origin)) == "spawner") {
        placeDungeonSpawner(chunk, random, origin);
    }
    return true;
}

/// parity: `EndPlatformFeature.createEndPlatform`: a 5x5 obsidian slab with three air layers over it.
bool placeEndPlatform(Chunk& chunk, const DecorationPos& origin) {
    for (std::int32_t dz = -2; dz <= 2; ++dz) {
        for (std::int32_t dx = -2; dx <= 2; ++dx) {
            for (std::int32_t dy = -1; dy < 3; ++dy) {
                set(chunk, offset(origin, dx, dy, dz), dy == -1 ? state("obsidian") : registry::blocks::air().stateId);
            }
        }
    }
    return true;
}

/// parity: `VoidStartPlatformFeature`: a fixed (8,8) centre, three blocks above the supplied y.
bool placeVoidPlatform(Chunk& chunk, const DecorationPos& origin) {
    if (checkerboardDistance(origin.x >> 4, origin.z >> 4, kPlatformChunkX, kPlatformChunkZ) > 1) {
        return true;
    }
    const std::int32_t platformY = origin.y + kPlatformYOffset;
    for (std::int32_t z = chunk.chunkZ() * 16; z <= chunk.chunkZ() * 16 + 15; ++z) {
        for (std::int32_t x = chunk.chunkX() * 16; x <= chunk.chunkX() * 16 + 15; ++x) {
            if (checkerboardDistance(kPlatformX, kPlatformZ, x, z) > kPlatformRadius) {
                continue;
            }
            const bool centre = x == kPlatformX && z == kPlatformZ;
            set(chunk, DecorationPos{.x = x, .y = platformY, .z = z}, centre ? state("cobblestone") : state("stone"));
        }
    }
    return true;
}

/// parity: `EndGatewayFeature`'s sparse 3x5x3 bedrock cross and its exact return target.
bool placeGateway(Chunk& chunk, const DecorationPos& origin) {
    const std::int32_t bedrock = state("bedrock");
    for (std::int32_t dy = -2; dy <= 2; ++dy) {
        for (std::int32_t dx = -1; dx <= 1; ++dx) {
            for (std::int32_t dz = -1; dz <= 1; ++dz) {
                const bool sameX = dx == 0;
                const bool sameY = dy == 0;
                const bool sameZ = dz == 0;
                const bool verticalTip = std::abs(dy) == 2;
                std::int32_t stateId = registry::blocks::air().stateId;
                if (sameX && sameY && sameZ) {
                    stateId = state("end_gateway");
                } else if (sameY) {
                    stateId = registry::blocks::air().stateId;
                } else if (verticalTip && sameX && sameZ) {
                    stateId = bedrock;
                } else if ((sameX || sameZ) && !verticalTip) {
                    stateId = bedrock;
                }
                set(chunk, offset(origin, dx, dy, dz), stateId);
            }
        }
    }
    if (local(chunk, origin)) {
        wire::nbt::NbtCompound data;
        data.putString("id", "minecraft:end_gateway");
        data.putLong("Age", 0LL);
        data.putIntArray("exit_portal", {100, 50, 0});
        data.putBoolean("ExactTeleport", true);
        data.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
        chunk.setBlockEntity(ChunkBlockEntity{origin.x & 15, origin.y, origin.z & 15,
                                              wire::common::Identifier::vanilla("end_gateway"), std::move(data)});
    }
    return true;
}

[[nodiscard]] std::int32_t lakeIndex(std::int32_t x, std::int32_t y, std::int32_t z) {
    return (x * kLakeWidth + z) * kLakeHeight + y;
}

[[nodiscard]] bool isLakeBoundary(const std::vector<bool>& grid, std::int32_t x, std::int32_t y, std::int32_t z) {
    const bool touchesCarve = (x < kLakeWidth - 1 && grid[static_cast<std::size_t>(lakeIndex(x + 1, y, z))]) ||
                              (x > 0 && grid[static_cast<std::size_t>(lakeIndex(x - 1, y, z))]) ||
                              (z < kLakeWidth - 1 && grid[static_cast<std::size_t>(lakeIndex(x, y, z + 1))]) ||
                              (z > 0 && grid[static_cast<std::size_t>(lakeIndex(x, y, z - 1))]) ||
                              (y < kLakeHeight - 1 && grid[static_cast<std::size_t>(lakeIndex(x, y + 1, z))]) ||
                              (y > 0 && grid[static_cast<std::size_t>(lakeIndex(x, y - 1, z))]);
    return !grid[static_cast<std::size_t>(lakeIndex(x, y, z))] && touchesCarve;
}

[[nodiscard]] bool canReplaceLakeBody(std::int32_t stateId) {
    return !featuresCannotReplace().contains(pathOf(stateId));
}

[[nodiscard]] bool canReplaceLakeBarrier(std::int32_t stateId) {
    return canReplaceLakeBody(stateId) && !registry::blockStateFamilies::isLeavesState(stateId) &&
           !tree::treeBlockRules::isLogState(stateId);
}

/// parity: `LakeFeature`: a union of four to seven ellipsoids in its fixed 16x8 mask, including the
/// boundary preflight, the cave-air and fluid carve, and the probabilistic solid barrier.
bool placeLake(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::int32_t fluid,
               std::int32_t barrier, std::string_view forbiddenBoundaryBlock = {}) {
    if (origin.y <= chunk.verticalBounds().minY() + 4) {
        return false;
    }
    const DecorationPos base = offset(origin, -8, -4, -8);
    std::vector<bool> grid(static_cast<std::size_t>(kLakeGridSize), false);
    const std::int32_t blobs = random.nextInt(4) + 4;
    for (std::int32_t blob = 0; blob < blobs; ++blob) {
        const double radiusX = random.nextDouble() * 6.0 + 3.0;
        const double radiusY = random.nextDouble() * 4.0 + 2.0;
        const double radiusZ = random.nextDouble() * 6.0 + 3.0;
        const double centreX = random.nextDouble() * (16.0 - radiusX - 2.0) + 1.0 + radiusX / 2.0;
        const double centreY = random.nextDouble() * (8.0 - radiusY - 4.0) + 2.0 + radiusY / 2.0;
        const double centreZ = random.nextDouble() * (16.0 - radiusZ - 2.0) + 1.0 + radiusZ / 2.0;
        for (std::int32_t x = 1; x < 15; ++x) {
            for (std::int32_t z = 1; z < 15; ++z) {
                for (std::int32_t y = 1; y < 7; ++y) {
                    const double dx = (static_cast<double>(x) - centreX) / (radiusX / 2.0);
                    const double dy = (static_cast<double>(y) - centreY) / (radiusY / 2.0);
                    const double dz = (static_cast<double>(z) - centreZ) / (radiusZ / 2.0);
                    if (dx * dx + dy * dy + dz * dz < 1.0) {
                        grid[static_cast<std::size_t>(lakeIndex(x, y, z))] = true;
                    }
                }
            }
        }
    }

    for (std::int32_t x = 0; x < kLakeWidth; ++x) {
        for (std::int32_t z = 0; z < kLakeWidth; ++z) {
            for (std::int32_t y = 0; y < kLakeHeight; ++y) {
                if (!isLakeBoundary(grid, x, y, z)) {
                    continue;
                }
                const std::int32_t stateId = stateAt(chunk, offset(base, x, y, z));
                if (y >= kLakeFluidHeight && liquidBlockPaths().contains(pathOf(stateId))) {
                    return false;
                }
                if (y < kLakeFluidHeight && !registry::blockShapes::isSolidState(stateId) && stateId != fluid) {
                    return false;
                }
                if (!forbiddenBoundaryBlock.empty() && pathOf(stateId) == forbiddenBoundaryBlock) {
                    return false;
                }
            }
        }
    }

    for (std::int32_t x = 0; x < kLakeWidth; ++x) {
        for (std::int32_t z = 0; z < kLakeWidth; ++z) {
            for (std::int32_t y = 0; y < kLakeHeight; ++y) {
                if (!grid[static_cast<std::size_t>(lakeIndex(x, y, z))]) {
                    continue;
                }
                const DecorationPos position = offset(base, x, y, z);
                if (canReplaceLakeBody(stateAt(chunk, position))) {
                    set(chunk, position, y >= kLakeFluidHeight ? state("cave_air") : fluid);
                }
            }
        }
    }

    if (!registry::blocks::isAir(barrier)) {
        for (std::int32_t x = 0; x < kLakeWidth; ++x) {
            for (std::int32_t z = 0; z < kLakeWidth; ++z) {
                for (std::int32_t y = 0; y < kLakeHeight; ++y) {
                    if (!isLakeBoundary(grid, x, y, z) || (y >= kLakeFluidHeight && random.nextInt(2) == 0)) {
                        continue;
                    }
                    const DecorationPos position = offset(base, x, y, z);
                    const std::int32_t stateId = stateAt(chunk, position);
                    if (registry::blockShapes::isSolidState(stateId) && canReplaceLakeBarrier(stateId)) {
                        set(chunk, position, barrier);
                    }
                }
            }
        }
    }

    // `lake_lava` never enters LakeFeature's water-freeze branch, and sulfur caves are temperature
    // 0.8, so sulfur_pool does not freeze in its actual biome placement either.
    return true;
}

[[nodiscard]] bool hasSourceWaterFluid(std::int32_t stateId) {
    const std::int32_t water = registry::blocks::water().stateId;
    return registry::blockFluids::hasWaterFluid(stateId) && !(stateId > water && stateId <= water + 15);
}

/// parity: sulfur_pool's `SequenceFeature`: a water lake, then the four-step downward wet vent.
bool placeSulfurPool(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    if (!placeLake(chunk, random, origin, registry::blocks::water().stateId, state("sulfur"), "sulfur_spike")) {
        return false;
    }
    DecorationPos position = origin;
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t step = 0; step <= kSulfurVentScanSteps; ++step) {
        const std::int32_t stateId = stateAt(chunk, position);
        if (registry::blockShapes::isSolidState(stateId) &&
            hasSourceWaterFluid(stateAt(chunk, offset(position, 0, 1, 0)))) {
            set(chunk, position, state("potent_sulfur", {{"potent_sulfur_state", "wet"}}));
            return true;
        }
        if (step < kSulfurVentScanSteps) {
            position.y--;
            if (position.y < bounds.minY() || position.y > bounds.maxY()) {
                return false;
            }
        }
    }
    return false;
}

/// parity: `VinesFeature`, which hangs a vine off whatever solid face it finds.
bool placeVines(Chunk& chunk, const DecorationPos& origin) {
    if (!registry::blocks::isAir(stateAt(chunk, origin))) {
        return false;
    }
    for (const VineFace& face : kVineFaces) {
        const std::int32_t supportStateId = stateAt(chunk, offset(origin, face.dx, face.dy, face.dz));
        if (!registry::blockShapes::isFaceSturdy(supportStateId, face.supportFace) &&
            !registry::blockShapes::isCollisionFaceFull(supportStateId, face.supportFace)) {
            continue;
        }
        const std::optional<std::int32_t> stateId = registry::blocks::stateIdOf(
            wire::common::Identifier::vanilla("vine"), {{std::string{face.property}, "true"}});
        if (!stateId.has_value()) {
            continue;
        }
        set(chunk, origin, *stateId);
        return true;
    }
    return false;
}

} // namespace

const std::set<std::string, std::less<>>& standaloneFeatures::placedIds() {
    static const std::set<std::string, std::less<>> ids = {"spring_closed",
                                                           "spring_closed_double",
                                                           "sulfur_pool",
                                                           "monster_room",
                                                           "monster_room_deep",
                                                           "end_platform",
                                                           "void_start_platform",
                                                           "end_gateway_return",
                                                           "lake_lava_surface",
                                                           "lake_lava_underground",
                                                           "cave_vines",
                                                           "classic_vines_cave_feature",
                                                           "lush_caves_vegetation",
                                                           "lush_caves_ceiling_vegetation",
                                                           "lush_caves_clay",
                                                           "rooted_azalea_tree"};
    return ids;
}

bool standaloneFeatures::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                                 const std::string_view placedFeature) {
    // parity: `spring_closed` is a lava spring walled in by five rocks and no hole.
    if (placedFeature == "spring_closed" || placedFeature == "spring_closed_double") {
        return placeSpring(chunk, origin, "netherrack", state("lava"), 5, 0);
    }
    if (placedFeature == "sulfur_pool") {
        return placeSulfurPool(chunk, random, origin);
    }
    if (placedFeature == "monster_room" || placedFeature == "monster_room_deep") {
        return placeMonsterRoom(chunk, random, origin);
    }
    if (placedFeature == "end_platform") {
        return placeEndPlatform(chunk, origin);
    }
    if (placedFeature == "void_start_platform") {
        return placeVoidPlatform(chunk, origin);
    }
    if (placedFeature == "end_gateway_return") {
        return placeGateway(chunk, origin);
    }
    if (placedFeature == "lake_lava_surface" || placedFeature == "lake_lava_underground") {
        return placeLake(chunk, random, origin, state("lava"), state("stone"));
    }
    if (placedFeature == "cave_vines") {
        return caveVineColumnFeature::place(chunk, random, origin);
    }
    if (placedFeature == "classic_vines_cave_feature") {
        return placeVines(chunk, origin);
    }
    if (placedFeature == "lush_caves_vegetation") {
        return lushVegetationPatchFeature::placeFloorMoss(chunk, random, origin);
    }
    if (placedFeature == "lush_caves_ceiling_vegetation") {
        return lushVegetationPatchFeature::placeCeilingMoss(chunk, random, origin);
    }
    if (placedFeature == "lush_caves_clay") {
        return lushVegetationPatchFeature::placeClay(chunk, random, origin);
    }
    if (placedFeature == "rooted_azalea_tree") {
        return rootedAzaleaFeature::place(chunk, random, origin);
    }
    return false;
}

} // namespace basalt::server::world::terrain::decoration
