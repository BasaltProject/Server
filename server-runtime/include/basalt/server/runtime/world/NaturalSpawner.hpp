#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/MobCategory.hpp>
#include <basalt/server/runtime/world/MobSpawnFinalization.hpp>
#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::world {
class Chunk;
}

namespace basalt::server::runtime::world {

class LoadedWorld;

/// A mob the spawner decided to make: what, where, and facing which way.
struct SpawnRequest {
    wire::common::Identifier type;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    /// Accepted members already finalized in this one pack; zero means the pack's first mob.
    std::int32_t groupIndex = 0;
    /// Biome at this member's actual block position, used by variant spawn rules.
    std::optional<wire::common::Identifier> biome;
};

/// What the world puts in an empty chunk while nobody is looking.
///
/// parity: `NaturalSpawner`. One position per chunk per tick, a pack of one to four around it of one
/// kind chosen by weight, and caps per category that scale with the chunks being simulated.
namespace naturalSpawner {

/// What one chunk's attempt needs of the world it cannot ask for itself.
struct Hooks {
    /// The list for a biome at a position. parity: `NaturalSpawner.mobsAt`, which takes a position
    /// because a structure standing there can replace the biome's list outright.
    std::function<std::vector<SpawnerEntry>(const wire::common::Identifier& biome, std::int32_t x, std::int32_t y,
                                            std::int32_t z)>
        entriesFor;
    /// How far the nearest player is, or nothing when there is nobody to spawn for.
    std::function<std::optional<double>(double x, double y, double z)> playerDistanceSquared;
    /// Whether a neighbouring chunk will take a mob, which is the loaded check by default.
    std::function<bool(std::int32_t chunkX, std::int32_t chunkZ)> canSpawnEntitiesInChunk;
    /// Runs at the exact attempt, so a rejected plugin or cost spawn does not consume the pack.
    std::function<bool(const SpawnRequest&)> requestAccepted;
};

/// parity: `NaturalSpawner`'s own 289, the chunks a cap is measured against.
inline constexpr std::int32_t kCapChunks = 289;

/// How many of `category` this world may hold, given how many chunks are being simulated.
///
/// parity: `NaturalSpawner.SpawnState.canSpawnForCategory`, `max * chunks / 289`.
[[nodiscard]] std::int32_t capFor(entity::MobCategory category, std::int32_t simulatedChunks);

/// parity: `getFilteredSpawningCategories`, including all four aquatic categories.
[[nodiscard]] std::vector<entity::MobCategory> categoriesFor(std::string_view difficulty, std::int64_t gameTime);

/// One chunk's worth of spawning. Answers what to spawn, which the caller puts in the world: this
/// decides, it does not create.
[[nodiscard]] std::vector<SpawnRequest> spawnForChunk(LoadedWorld& world, std::int32_t chunkX, std::int32_t chunkZ,
                                                      entity::MobCategory category, const Hooks& hooks,
                                                      random::RandomSource& random);

/// parity: mob overrides of `getMaxSpawnClusterSize`, default four.
[[nodiscard]] std::int32_t maxClusterSize(const wire::common::Identifier& type);

/// parity: `EntityType.canSpawnFarFromPlayer` before a natural mob is constructed.
[[nodiscard]] bool canSpawnAtDistance(const wire::common::Identifier& type, entity::MobCategory fallbackCategory,
                                      double distanceSquared);

/// parity: `WeightedList<SpawnerData>.contains`; the weight belongs to the wrapper, not the record.
[[nodiscard]] bool containsSpawnData(const std::vector<SpawnerEntry>& entries, const SpawnerEntry& selected);

/// parity: `SpawnPlacementTypes.ON_GROUND` plus each category's own light rule: a monster needs the
/// dark, while the selected type decides ground, water or lava placement.
///
/// The entity type matters independently of its cap category: drowned are monsters that spawn in
/// water, and striders are creatures that spawn in lava. A null type asks the category alone.
[[nodiscard]] bool canSpawnAt(LoadedWorld& world, const wire::common::Identifier* type, entity::MobCategory category,
                              std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random);

/// parity: `NaturalSpawner.isValidEmptySpawnBlock`, for custom spawners at one position.
[[nodiscard]] bool isValidEmptySpawnBlock(LoadedWorld& world, const wire::common::Identifier& type, std::int32_t x,
                                          std::int32_t y, std::int32_t z);

/// parity: `Mob.checkMobSpawnRules`, whose patrol path only asks about the block below.
[[nodiscard]] bool canSpawnOnGround(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] bool canSeeSky(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: the constant eight-keyframe `timeline/moon` gameplay track.
[[nodiscard]] float surfaceSlimeChance(std::int64_t clockTicks);

/// parity: `WorldgenRandom.seedSlimeChunk`, including Java's intentional int overflow.
[[nodiscard]] bool isSlimeChunk(std::int64_t worldSeed, std::int32_t chunkX, std::int32_t chunkZ);

/// parity: `WeightedRandomList.getRandom` over the biome's own weights.
[[nodiscard]] const SpawnerEntry* pick(const std::vector<SpawnerEntry>& entries, random::RandomSource& random);

/// parity: 26.3 `SpawnerData.count().sample`: a constant draws nothing, a uniform range one `nextInt`.
[[nodiscard]] std::int32_t packSize(const SpawnerEntry& entry, random::RandomSource& random);

/// parity: the biome at the position, which is what decides the list to pick from.
[[nodiscard]] std::optional<wire::common::Identifier> biomeAt(LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                              std::int32_t z);

/// parity: `getRandomPosWithin` samples through `WORLD_SURFACE` plus one extra block.
[[nodiscard]] std::int32_t spawnTopY(const server::world::Chunk& chunk, std::int32_t localX, std::int32_t localZ);

} // namespace naturalSpawner

} // namespace basalt::server::runtime::world
