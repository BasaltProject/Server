#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/registry/VanillaBiomes.hpp>
#include <basalt/server/runtime/world/NaturalSpawnObstruction.hpp>
#include <basalt/server/runtime/world/NaturalSpawnPredicates.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/server/runtime/world/OriginalMobSpawner.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::world::originalMobSpawner {
namespace {

namespace reg = server::registry;

using server::world::Chunk;
using wire::common::Identifier;
using wire::nbt::NbtCompound;

constexpr std::int32_t kPositionAttempts = 4;
constexpr std::int32_t kPackSpread = 5;
constexpr std::int32_t kSeaLevel = 63;

[[nodiscard]] std::string_view blockPathOf(std::int32_t stateId) {
    const Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string_view{} : std::string_view{block->path()};
}

[[nodiscard]] bool isAir(std::int32_t stateId) { return blockPathOf(stateId) == "air"; }

[[nodiscard]] bool isLava(std::int32_t stateId) { return blockPathOf(stateId) == "lava"; }

[[nodiscard]] std::optional<Identifier> biomeAt(const Chunk& chunk, std::int32_t worldX, std::int32_t y,
                                                std::int32_t worldZ) {
    const server::world::ChunkHeight bounds = chunk.verticalBounds();
    const std::int32_t quartY = std::clamp((y - bounds.minY()) >> 2, 0, bounds.height() / 4 - 1);
    const std::int32_t biomeId = chunk.biomeId((worldX & 15) >> 2, quartY, (worldZ & 15) >> 2);
    const std::vector<registry::vanillaBiomes::Entry>& all = registry::vanillaBiomes::all();
    if (biomeId < 0 || static_cast<std::size_t>(biomeId) >= all.size()) {
        return std::nullopt;
    }
    return Identifier::vanilla(all[static_cast<std::size_t>(biomeId)].path);
}

/// parity: the heightmap a pack starts from, and the Nether's own walk down from the ceiling.
[[nodiscard]] std::int32_t topPosition(const Chunk& chunk, std::string_view type, std::int32_t worldX,
                                       std::int32_t worldZ, bool hasCeiling) {
    const server::world::HeightmapType heightmap = type == "ocelot" || type == "parrot"
                                                       ? server::world::HeightmapType::MotionBlocking
                                                       : server::world::HeightmapType::MotionBlockingNoLeaves;
    std::int32_t y = chunk.heightAt(worldX & 15, worldZ & 15, heightmap);
    if (!hasCeiling) {
        return y;
    }
    const std::int32_t localX = worldX & 15;
    const std::int32_t localZ = worldZ & 15;
    const std::int32_t minY = chunk.verticalBounds().minY();
    do {
        y--;
    } while (y > minY && !isAir(chunk.blockStateId(localX, y, localZ)));
    do {
        y--;
    } while (y > minY && isAir(chunk.blockStateId(localX, y, localZ)));
    return y;
}

[[nodiscard]] bool canSpawnStrider(const Chunk& chunk, std::int32_t worldX, std::int32_t y, std::int32_t worldZ) {
    const std::int32_t localX = worldX & 15;
    const std::int32_t localZ = worldZ & 15;
    if (!isLava(chunk.blockStateId(localX, y, localZ))) {
        return false;
    }
    for (std::int32_t aboveY = y + 1; aboveY <= chunk.verticalBounds().maxY(); aboveY++) {
        const std::int32_t above = chunk.blockStateId(localX, aboveY, localZ);
        if (!isLava(above)) {
            return isAir(above);
        }
    }
    return false;
}

[[nodiscard]] bool canSpawnAt(const Chunk& chunk, std::string_view type, std::int32_t worldX, std::int32_t y,
                              std::int32_t worldZ, std::string_view biome) {
    if (y <= chunk.verticalBounds().minY() || y >= chunk.verticalBounds().maxY()) {
        return false;
    }
    if (type == "strider") {
        return canSpawnStrider(chunk, worldX, y, worldZ);
    }
    const std::int32_t ground = chunk.blockStateId(worldX & 15, y - 1, worldZ & 15);
    if (naturalSpawnPredicates::usesGroundPlacement(type) &&
        !reg::blockShapes::isFaceSturdy(ground, reg::BlockFace::Up)) {
        return false;
    }
    const std::int32_t feet = chunk.blockStateId(worldX & 15, y, worldZ & 15);
    const std::int32_t head = chunk.blockStateId(worldX & 15, y + 1, worldZ & 15);
    if (!reg::blockShapes::isEmptyShape(feet) || !reg::blockShapes::isEmptyShape(head)) {
        return false;
    }
    const std::string_view groundPath = blockPathOf(ground);
    if (groundPath.empty()) {
        return false;
    }
    return naturalSpawnPredicates::allows(type, groundPath, reg::blockStateFamilies::isLeavesState(ground), biome, y,
                                          kSeaLevel);
}

} // namespace

std::int32_t spawnInto(Chunk& chunk, const std::vector<SpawnerEntry>& entries, float creatureProbability,
                       random::RandomSource& random, const Options& options) {
    if (entries.empty()) {
        return 0;
    }
    const std::int32_t minX = chunk.chunkX() << 4;
    const std::int32_t minZ = chunk.chunkZ() << 4;
    std::int32_t added = 0;
    while (random.nextFloat() < creatureProbability) {
        const SpawnerEntry* entry = naturalSpawner::pick(entries, random);
        if (entry == nullptr) {
            break;
        }
        const std::int32_t count = naturalSpawner::packSize(*entry, random);
        std::int32_t x = minX + random.nextInt(16);
        std::int32_t z = minZ + random.nextInt(16);
        const std::int32_t startX = x;
        const std::int32_t startZ = z;
        std::int32_t groupSize = 0;
        MobSpawnGroupData group;
        for (std::int32_t member = 0; member < count; member++) {
            bool success = false;
            std::int32_t attempts = 0;
            while (!success && attempts < kPositionAttempts) {
                attempts++;
                const std::int32_t y = topPosition(chunk, entry->type.path(), x, z, options.hasCeiling);
                const double width = reg::entityTypeShapes::widthOf(entry->type);
                const double entityX = std::clamp(static_cast<double>(x), minX + width, minX + 16.0 - width);
                const double entityZ = std::clamp(static_cast<double>(z), minZ + width, minZ + 16.0 - width);
                const auto blockX = static_cast<std::int32_t>(entityX);
                const auto blockZ = static_cast<std::int32_t>(entityZ);
                const std::optional<Identifier> biome = biomeAt(chunk, blockX, y, blockZ);
                const bool placeable =
                    canSpawnAt(chunk, entry->type.path(), blockX, y, blockZ,
                               biome.has_value() ? std::string_view{biome->path()} : std::string_view{}) &&
                    naturalSpawnObstruction::blocksAllow(
                        entry->type, entityX, static_cast<double>(y), entityZ,
                        [&chunk](std::int32_t wx, std::int32_t wy, std::int32_t wz) -> std::optional<std::int32_t> {
                            if ((wx >> 4) != chunk.chunkX() || (wz >> 4) != chunk.chunkZ() ||
                                wy < chunk.verticalBounds().minY() || wy > chunk.verticalBounds().maxY()) {
                                return std::nullopt;
                            }
                            return chunk.blockStateId(wx & 15, wy, wz & 15);
                        });
                if (placeable) {
                    const float yaw = random.nextFloat() * 360.0F;
                    NbtCompound data;
                    MobSpawnContext context;
                    context.random = &random;
                    context.difficulty = options.difficulty;
                    context.equipment = options.equipment;
                    context.biome = biome;
                    context.gameTime = options.gameTime;
                    context.reason = MobSpawnReason::ChunkGeneration;
                    context.entityRandomFactory = options.entityRandomFactory;
                    const MobSpawnForest forest =
                        writeGeneratedForest(data, entry->type, groupSize, context, group, blockX, y, blockZ, entityX,
                                             static_cast<double>(y), entityZ, yaw);
                    for (const server::world::ChunkGeneratedEntity& root : forest.roots) {
                        chunk.addGeneratedEntity(root);
                    }
                    added++;
                    groupSize++;
                    success = true;
                }
                x += random.nextInt(kPackSpread) - random.nextInt(kPackSpread);
                z += random.nextInt(kPackSpread) - random.nextInt(kPackSpread);
                while (x < minX || x >= minX + 16 || z < minZ || z >= minZ + 16) {
                    x = startX + random.nextInt(kPackSpread) - random.nextInt(kPackSpread);
                    z = startZ + random.nextInt(kPackSpread) - random.nextInt(kPackSpread);
                }
            }
        }
    }
    return added;
}

} // namespace basalt::server::runtime::world::originalMobSpawner
