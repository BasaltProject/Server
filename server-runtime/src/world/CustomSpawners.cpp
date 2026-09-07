#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/CustomSpawners.hpp>
#include <basalt/server/runtime/world/DaylightDetectors.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobLocalDifficulty.hpp>
#include <basalt/server/runtime/world/VillageProximity.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/server/world/light/Light.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

namespace reg = server::registry;

using wire::common::Identifier;

[[nodiscard]] std::int32_t blockOf(double coordinate) { return static_cast<std::int32_t>(std::floor(coordinate)); }

[[nodiscard]] const Identifier& phantom() {
    static const Identifier type = Identifier::vanilla("phantom");
    return type;
}

[[nodiscard]] const Identifier& cat() {
    static const Identifier type = Identifier::vanilla("cat");
    return type;
}

/// parity: `Difficulty.getId`, which is what decides how many phantoms one roll makes.
[[nodiscard]] std::int32_t difficultyId(std::string_view difficulty) {
    if (difficulty == "peaceful") {
        return 0;
    }
    if (difficulty == "normal") {
        return 2;
    }
    return difficulty == "hard" ? 3 : 1;
}

/// parity: the moon phase as `Level.getMoonBrightness` answers it, which is the slime curve doubled.
[[nodiscard]] float moonBrightness(const LoadedWorld& world) {
    return naturalSpawner::surfaceSlimeChance(world.clock().totalTicks()) * 2.0F;
}

} // namespace

void PhantomSpawner::tick(LoadedWorld& world, bool spawnEnemies, const std::vector<player::PlayerSession*>& players,
                          random::RandomSource& random, const Spawn& spawn) {
    constexpr std::int32_t kTicksPerSecond = 20;
    constexpr std::int32_t kMinIntervalSeconds = 60;
    constexpr std::int32_t kIntervalSecondsRange = 60;
    constexpr std::int32_t kMinSkyDarken = 5;
    constexpr float kMaxDifficultyRoll = 3.0F;
    constexpr std::int32_t kMinTimeSinceRest = 72000;
    constexpr std::int32_t kMinHeight = 20;
    constexpr std::int32_t kHeightRange = 15;
    constexpr std::int32_t kHorizontalRadius = 10;
    constexpr std::int32_t kHorizontalDiameter = 21;

    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    if (world.declaration().dimension != config::WorldDimension::Overworld || !spawnEnemies ||
        !world.gameRules().get(gameRules::kSpawnPhantoms)) {
        return;
    }
    nextTick_--;
    if (nextTick_ > 0) {
        return;
    }
    nextTick_ += (kMinIntervalSeconds + random.nextInt(kIntervalSecondsRange)) * kTicksPerSecond;
    if (facts.hasSkylight && daylightDetectors::skyDarken(world.clock(), world.weather()) < kMinSkyDarken) {
        return;
    }

    for (const player::PlayerSession* player : players) {
        if (player->gamemode == wire::protocol::play::Gamemode::Spectator) {
            continue;
        }
        const std::int32_t playerX = blockOf(player->x);
        const std::int32_t playerY = blockOf(player->y);
        const std::int32_t playerZ = blockOf(player->z);
        if (facts.hasSkylight &&
            (playerY < facts.seaLevel || !naturalSpawner::canSeeSky(world, playerX, playerY, playerZ))) {
            continue;
        }

        const server::world::Chunk* chunk = world.state().chunkIfLoaded(playerX >> 4, playerZ >> 4);
        const std::int64_t inhabitedTime = chunk == nullptr ? 0 : chunk->inhabitedTime();
        const float effectiveDifficulty = mobLocalDifficulty::effectiveDifficulty(world.difficulty(), world.gameTime(),
                                                                                  inhabitedTime, moonBrightness(world));
        if (effectiveDifficulty <= random.nextFloat() * kMaxDifficultyRoll) {
            continue;
        }

        const auto timeSinceRest = static_cast<std::int32_t>(std::clamp<std::int64_t>(
            player->stats.get(storage::stats::timeSinceRest()), 1, std::numeric_limits<std::int32_t>::max()));
        if (random.nextInt(timeSinceRest) < kMinTimeSinceRest) {
            continue;
        }

        const std::int32_t spawnX = playerX - kHorizontalRadius + random.nextInt(kHorizontalDiameter);
        const std::int32_t spawnY = playerY + kMinHeight + random.nextInt(kHeightRange);
        const std::int32_t spawnZ = playerZ - kHorizontalRadius + random.nextInt(kHorizontalDiameter);
        if (!naturalSpawner::isValidEmptySpawnBlock(world, phantom(), spawnX, spawnY, spawnZ)) {
            continue;
        }

        const std::int32_t groupSize = 1 + random.nextInt(difficultyId(world.difficulty()) + 1);
        for (std::int32_t groupIndex = 0; groupIndex < groupSize; groupIndex++) {
            spawn(SpawnRequest{.type = phantom(),
                               .x = spawnX + 0.5,
                               .y = static_cast<double>(spawnY),
                               .z = spawnZ + 0.5,
                               .yaw = 0.0F,
                               .groupIndex = groupIndex,
                               .biome = naturalSpawner::biomeAt(world, spawnX, spawnY, spawnZ)});
        }
    }
}

CatSpawner::CatSpawner(InStructureAt catsSpawnInStructureAt) : inStructureAt_(std::move(catsSpawnInStructureAt)) {}

void CatSpawner::tick(LoadedWorld& world, const std::vector<player::PlayerSession*>& players,
                      random::RandomSource& random, const Spawn& spawn) {
    constexpr std::int32_t kTickDelay = 1200;
    constexpr std::int32_t kMinPlayerOffset = 8;
    constexpr std::int32_t kPlayerOffsetRange = 24;
    constexpr std::int32_t kLoadedRadius = 10;
    constexpr std::int32_t kVillageSectionDistance = 2;
    constexpr std::int32_t kVillageRadius = 48;
    constexpr std::int32_t kHutRadius = 16;
    constexpr std::int32_t kVerticalRadius = 8;
    constexpr std::int32_t kMinOccupiedHomes = 4;
    constexpr std::int32_t kMaxVillageCats = 5;
    constexpr std::int32_t kMaxGroundLightEmission = 14;

    if (world.declaration().dimension != config::WorldDimension::Overworld) {
        return;
    }
    nextTick_--;
    if (nextTick_ > 0) {
        return;
    }
    nextTick_ = kTickDelay;

    std::vector<const player::PlayerSession*> alive;
    for (const player::PlayerSession* player : players) {
        if (!player->dead && player->health() > 0.0F) {
            alive.push_back(player);
        }
    }
    if (alive.empty()) {
        return;
    }
    const player::PlayerSession& player =
        *alive[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(alive.size())))];

    const auto signedOffset = [&random] {
        const std::int32_t distance = kMinPlayerOffset + random.nextInt(kPlayerOffsetRange);
        return random.nextBoolean() ? -distance : distance;
    };
    const std::int32_t x = blockOf(player.x) + signedOffset();
    const std::int32_t y = blockOf(player.y);
    const std::int32_t z = blockOf(player.z) + signedOffset();

    // parity: the whole square has to be loaded, which is what keeps a cat off a chunk border.
    for (std::int32_t chunkX = (x - kLoadedRadius) >> 4; chunkX <= ((x + kLoadedRadius) >> 4); chunkX++) {
        for (std::int32_t chunkZ = (z - kLoadedRadius) >> 4; chunkZ <= ((z + kLoadedRadius) >> 4); chunkZ++) {
            if (world.state().chunkIfLoaded(chunkX, chunkZ) == nullptr) {
                return;
            }
        }
    }

    // parity: `SpawnPlacementTypes.ON_GROUND`; `CatSpawner` deliberately skips the animal predicate.
    if (world.border().distanceOutside(x + 0.5, z + 0.5) > 0.0) {
        return;
    }
    const std::optional<std::int32_t> ground = world.state().blockState(x, y - 1, z);
    if (!ground.has_value() || !reg::blockShapes::isFaceSturdy(*ground, reg::BlockFace::Up) ||
        server::world::light::blockLight::emission(*ground) >= kMaxGroundLightEmission ||
        !naturalSpawner::isValidEmptySpawnBlock(world, cat(), x, y, z) ||
        !naturalSpawner::isValidEmptySpawnBlock(world, cat(), x, y + 1, z)) {
        return;
    }

    const auto catsIn = [&world, x, y, z](std::int32_t horizontalRadius) {
        const entity::Aabb search{x - static_cast<double>(horizontalRadius),
                                  y - static_cast<double>(kVerticalRadius),
                                  z - static_cast<double>(horizontalRadius),
                                  x + 1.0 + horizontalRadius,
                                  y + 1.0 + kVerticalRadius,
                                  z + 1.0 + horizontalRadius};
        std::int32_t found = 0;
        for (const entity::ServerEntity* entity : world.state().entities()) {
            if (entity == nullptr || entity->removed || entity->type() != cat()) {
                continue;
            }
            const entity::Aabb box =
                entity::Aabb::of(entity->x, entity->y, entity->z, reg::entityTypeShapes::widthOf(cat()),
                                 reg::entityTypeShapes::heightOf(cat()));
            if (box.intersects(search)) {
                found++;
            }
        }
        return found;
    };

    const auto request = [&world, x, y, z](bool persistent, bool inHut) {
        return CatSpawnRequest{
            .x = x + 0.5,
            .y = static_cast<double>(y),
            .z = z + 0.5,
            .persistent = persistent,
            .context = CatSpawnContext{.moonBrightness = moonBrightness(world), .inBlackCatStructure = inHut},
            .biome = naturalSpawner::biomeAt(world, x, y, z)};
    };

    if (villageProximity::isCloseToVillage(world.state(), x, y, z, kVillageSectionDistance)) {
        if (villageProximity::occupiedHomesNear(world.state(), x, y, z, kVillageRadius) <= kMinOccupiedHomes ||
            catsIn(kVillageRadius) >= kMaxVillageCats) {
            return;
        }
        spawn(request(false, false));
    } else if (inStructureAt_ && inStructureAt_(world, x, y, z)) {
        if (catsIn(kHutRadius) != 0) {
            return;
        }
        spawn(request(true, true));
    }
}

void PatrolSpawner::tick(LoadedWorld& world, bool spawnEnemies, const std::vector<player::PlayerSession*>& players,
                         random::RandomSource& random, const Spawn& spawn) {
    constexpr std::int32_t kBaseIntervalTicks = 12000;
    constexpr std::int32_t kIntervalRangeTicks = 1200;
    constexpr std::int32_t kAttemptOdds = 5;
    constexpr std::int32_t kBrightOutsideDarken = 4;
    constexpr std::int32_t kVillageSectionDistance = 2;
    constexpr std::int32_t kMinPlayerOffset = 24;
    constexpr std::int32_t kPlayerOffsetRange = 24;
    constexpr std::int32_t kLoadedRadius = 10;
    constexpr std::int32_t kMemberSpread = 5;
    constexpr std::int32_t kMaxBlockLight = 8;
    constexpr std::int64_t kPatrolsEnabledAt = 120000;

    if (world.declaration().dimension != config::WorldDimension::Overworld || !spawnEnemies ||
        !world.gameRules().get(gameRules::kSpawnPatrols)) {
        return;
    }
    nextTick_--;
    if (nextTick_ > 0) {
        return;
    }
    nextTick_ += kBaseIntervalTicks + random.nextInt(kIntervalRangeTicks);
    if (daylightDetectors::skyDarken(world.clock(), world.weather()) >= kBrightOutsideDarken ||
        random.nextInt(kAttemptOdds) != 0 || players.empty()) {
        return;
    }

    const player::PlayerSession& player =
        *players[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(players.size())))];
    if (player.gamemode == wire::protocol::play::Gamemode::Spectator) {
        return;
    }
    const std::int32_t playerX = blockOf(player.x);
    const std::int32_t playerY = blockOf(player.y);
    const std::int32_t playerZ = blockOf(player.z);
    if (villageProximity::isCloseToVillage(world.state(), playerX, playerY, playerZ, kVillageSectionDistance)) {
        return;
    }

    const auto signedOffset = [&random] {
        const std::int32_t distance = kMinPlayerOffset + random.nextInt(kPlayerOffsetRange);
        return random.nextBoolean() ? -distance : distance;
    };
    std::int32_t spawnX = playerX + signedOffset();
    std::int32_t spawnZ = playerZ + signedOffset();
    for (std::int32_t chunkX = (spawnX - kLoadedRadius) >> 4; chunkX <= ((spawnX + kLoadedRadius) >> 4); chunkX++) {
        for (std::int32_t chunkZ = (spawnZ - kLoadedRadius) >> 4; chunkZ <= ((spawnZ + kLoadedRadius) >> 4); chunkZ++) {
            if (world.state().chunkIfLoaded(chunkX, chunkZ) == nullptr) {
                return;
            }
        }
    }
    const std::optional<Identifier> initialBiome = naturalSpawner::biomeAt(world, spawnX, playerY, spawnZ);
    // parity: no patrols before the fifth day, and never over mushroom fields.
    if (world.clock().totalTicks() < kPatrolsEnabledAt ||
        (initialBiome.has_value() && initialBiome->path() == "mushroom_fields")) {
        return;
    }

    const server::world::Chunk* difficultyChunk = world.state().chunkIfLoaded(spawnX >> 4, spawnZ >> 4);
    const float effectiveDifficulty = mobLocalDifficulty::effectiveDifficulty(
        world.difficulty(), world.gameTime(), difficultyChunk == nullptr ? 0 : difficultyChunk->inhabitedTime(),
        moonBrightness(world));
    const auto groupSize = static_cast<std::int32_t>(std::ceil(static_cast<double>(effectiveDifficulty))) + 1;
    for (std::int32_t index = 0; index < groupSize; index++) {
        const server::world::Chunk* chunk = world.state().chunkIfLoaded(spawnX >> 4, spawnZ >> 4);
        if (chunk != nullptr) {
            const std::int32_t spawnY =
                chunk->heightAt(spawnX & 15, spawnZ & 15, server::world::HeightmapType::MotionBlockingNoLeaves);
            static const Identifier pillager = Identifier::vanilla("pillager");
            const bool placeable = naturalSpawner::isValidEmptySpawnBlock(world, pillager, spawnX, spawnY, spawnZ) &&
                                   worldLight::blockLight(world.state(), spawnX, spawnY, spawnZ) <= kMaxBlockLight &&
                                   naturalSpawner::canSpawnOnGround(world, spawnX, spawnY, spawnZ);
            const bool accepted =
                placeable && spawn &&
                spawn(PatrolSpawnRequest{.x = static_cast<double>(spawnX),
                                         .y = static_cast<double>(spawnY),
                                         .z = static_cast<double>(spawnZ),
                                         .leader = index == 0,
                                         .biome = naturalSpawner::biomeAt(world, spawnX, spawnY, spawnZ)});
            // parity: no captain, no patrol: the rest are abandoned rather than spawned leaderless.
            if (index == 0 && !accepted) {
                return;
            }
        }
        spawnX += random.nextInt(kMemberSpread) - random.nextInt(kMemberSpread);
        spawnZ += random.nextInt(kMemberSpread) - random.nextInt(kMemberSpread);
    }
}

namespace {

/// A block position, which the trader search passes around by value.
struct BlockPosition {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

[[nodiscard]] const Identifier& wanderingTrader() {
    static const Identifier type = Identifier::vanilla("wandering_trader");
    return type;
}

/// parity: `PoiManager.find(MEETING)`: the first bell in chunk and section traversal order, not the
/// closest one. Which bell it is decides where the trader appears, so the order is observable.
[[nodiscard]] std::optional<BlockPosition> findMeetingBell(LoadedWorld& world, BlockPosition centre,
                                                           std::int32_t radius) {
    const std::int32_t radiusSquared = radius * radius;
    const std::int32_t chunkRadius = radius / 16 + 1;
    const std::int32_t centreChunkX = centre.x >> 4;
    const std::int32_t centreChunkZ = centre.z >> 4;
    for (std::int32_t chunkZ = centreChunkZ - chunkRadius; chunkZ <= centreChunkZ + chunkRadius; chunkZ++) {
        for (std::int32_t chunkX = centreChunkX - chunkRadius; chunkX <= centreChunkX + chunkRadius; chunkX++) {
            const server::world::Chunk* chunk = world.state().chunkIfLoaded(chunkX, chunkZ);
            if (chunk == nullptr) {
                continue;
            }
            const std::int32_t minX = std::max(centre.x - radius, chunkX * 16);
            const std::int32_t maxX = std::min(centre.x + radius, chunkX * 16 + 15);
            const std::int32_t minZ = std::max(centre.z - radius, chunkZ * 16);
            const std::int32_t maxZ = std::min(centre.z + radius, chunkZ * 16 + 15);
            const std::int32_t minY = std::max(centre.y - radius, chunk->verticalBounds().minY());
            const std::int32_t maxY = std::min(centre.y + radius, chunk->verticalBounds().maxY());
            for (std::int32_t y = minY; y <= maxY; y++) {
                for (std::int32_t z = minZ; z <= maxZ; z++) {
                    for (std::int32_t x = minX; x <= maxX; x++) {
                        const std::int32_t dx = x - centre.x;
                        const std::int32_t dy = y - centre.y;
                        const std::int32_t dz = z - centre.z;
                        if (dx * dx + dy * dy + dz * dz > radiusSquared) {
                            continue;
                        }
                        const Identifier* block =
                            reg::blockStateDefinitions::blockOf(chunk->blockStateId(x & 15, y, z & 15));
                        if (block != nullptr && block->path() == "bell") {
                            return BlockPosition{x, y, z};
                        }
                    }
                }
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool isOnGroundSpawnPosition(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    constexpr std::int32_t kMaxGroundEmission = 14;
    if (world.border().distanceOutside(x + 0.5, z + 0.5) > 0.0) {
        return false;
    }
    const std::optional<std::int32_t> ground = world.state().blockState(x, y - 1, z);
    if (!ground.has_value() || !reg::blockShapes::isFaceSturdy(*ground, reg::BlockFace::Up) ||
        server::world::light::blockLight::emission(*ground) >= kMaxGroundEmission) {
        return false;
    }
    return naturalSpawner::isValidEmptySpawnBlock(world, wanderingTrader(), x, y, z) &&
           naturalSpawner::isValidEmptySpawnBlock(world, wanderingTrader(), x, y + 1, z);
}

/// parity: ten signed-square probes through the `MOTION_BLOCKING_NO_LEAVES` heightmap.
[[nodiscard]] std::optional<BlockPosition> findSpawnPositionNear(LoadedWorld& world, BlockPosition reference,
                                                                 std::int32_t radius, random::RandomSource& random) {
    constexpr std::int32_t kSpawnAttempts = 10;
    for (std::int32_t attempt = 0; attempt < kSpawnAttempts; attempt++) {
        const std::int32_t x = reference.x + random.nextInt(radius * 2) - radius;
        const std::int32_t z = reference.z + random.nextInt(radius * 2) - radius;
        const server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
        if (chunk == nullptr) {
            continue;
        }
        const std::int32_t y = chunk->heightAt(x & 15, z & 15, server::world::HeightmapType::MotionBlockingNoLeaves);
        if (isOnGroundSpawnPosition(world, x, y, z)) {
            return BlockPosition{x, y, z};
        }
    }
    return std::nullopt;
}

/// parity: the separate inclusive two-by-three-by-two volume checked only for the trader itself.
[[nodiscard]] bool hasEnoughSpace(LoadedWorld& world, BlockPosition position) {
    for (std::int32_t x = position.x; x <= position.x + 1; x++) {
        for (std::int32_t y = position.y; y <= position.y + 2; y++) {
            for (std::int32_t z = position.z; z <= position.z + 1; z++) {
                const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
                if (!state.has_value() || !reg::blockShapes::isEmptyShape(*state)) {
                    return false;
                }
            }
        }
    }
    return true;
}

} // namespace

WanderingTraderSpawner::WanderingTraderSpawner(WanderingTraderSpawnState state, random::RandomSource random)
    : state_(state), random_(random) {}

void WanderingTraderSpawner::tick(LoadedWorld& world, const std::vector<player::PlayerSession*>& players,
                                  random::RandomSource& levelRandom, const SpawnTrader& spawnTrader,
                                  const SpawnLlama& spawnLlama) {
    constexpr std::int32_t kDefaultTickDelay = 1200;
    constexpr std::int32_t kDefaultSpawnDelay = 24000;
    constexpr std::int32_t kMinSpawnChance = 25;
    constexpr std::int32_t kMaxSpawnChance = 75;
    constexpr std::int32_t kSpawnChanceIncrease = 25;
    constexpr std::int32_t kPercentRange = 100;

    if (world.declaration().dimension != config::WorldDimension::Overworld ||
        !world.gameRules().get(gameRules::kSpawnWanderingTraders)) {
        return;
    }
    tickDelay_--;
    if (tickDelay_ > 0) {
        return;
    }
    tickDelay_ = kDefaultTickDelay;
    state_.spawnDelay -= kDefaultTickDelay;
    if (state_.spawnDelay > 0) {
        return;
    }
    state_.spawnDelay = kDefaultSpawnDelay;
    const std::int32_t chanceToSpawn = state_.spawnChance;
    state_.spawnChance = std::clamp(chanceToSpawn + kSpawnChanceIncrease, kMinSpawnChance, kMaxSpawnChance);
    if (random_.nextInt(kPercentRange) > chanceToSpawn) {
        return;
    }
    if (spawn(world, players, levelRandom, spawnTrader, spawnLlama)) {
        state_.spawnChance = kMinSpawnChance;
    }
}

bool WanderingTraderSpawner::spawn(LoadedWorld& world, const std::vector<player::PlayerSession*>& players,
                                   random::RandomSource& levelRandom, const SpawnTrader& spawnTrader,
                                   const SpawnLlama& spawnLlama) {
    constexpr std::int32_t kSpawnOneInXChance = 10;
    constexpr std::int32_t kReferenceRadius = 48;
    constexpr std::int32_t kLlamaRadius = 4;
    constexpr std::int32_t kLlamaCount = 2;

    std::vector<const player::PlayerSession*> alive;
    for (const player::PlayerSession* player : players) {
        if (!player->dead && player->health() > 0.0F) {
            alive.push_back(player);
        }
    }
    // parity: nobody to arrive for counts as a success, which resets the chance rather than raising
    // it on an empty server.
    if (alive.empty()) {
        return true;
    }
    const player::PlayerSession& player =
        *alive[static_cast<std::size_t>(levelRandom.nextInt(static_cast<std::int32_t>(alive.size())))];
    if (random_.nextInt(kSpawnOneInXChance) != 0) {
        return false;
    }

    const BlockPosition playerPos{blockOf(player.x), blockOf(player.y), blockOf(player.z)};
    const BlockPosition reference = findMeetingBell(world, playerPos, kReferenceRadius).value_or(playerPos);
    const std::optional<BlockPosition> traderPos = findSpawnPositionNear(world, reference, kReferenceRadius, random_);
    if (!traderPos.has_value() || !hasEnoughSpace(world, *traderPos)) {
        return false;
    }
    const std::optional<Identifier> biome = naturalSpawner::biomeAt(world, traderPos->x, traderPos->y, traderPos->z);
    if (biome.has_value() && biome->path() == "the_void") {
        return false;
    }

    const std::optional<std::int32_t> traderId =
        spawnTrader ? spawnTrader(WanderingTraderSpawnRequest{.x = traderPos->x + 0.5,
                                                              .y = static_cast<double>(traderPos->y),
                                                              .z = traderPos->z + 0.5,
                                                              .referenceX = reference.x,
                                                              .referenceY = reference.y,
                                                              .referenceZ = reference.z})
                    : std::nullopt;
    if (!traderId.has_value()) {
        return false;
    }
    for (std::int32_t index = 0; index < kLlamaCount; index++) {
        const std::optional<BlockPosition> llamaPos = findSpawnPositionNear(world, *traderPos, kLlamaRadius, random_);
        if (llamaPos.has_value() && spawnLlama) {
            spawnLlama(TraderLlamaSpawnRequest{.x = llamaPos->x + 0.5,
                                               .y = static_cast<double>(llamaPos->y),
                                               .z = llamaPos->z + 0.5},
                       *traderId);
        }
    }
    return true;
}

namespace {

constexpr std::int32_t kSiegeBrightOutsideDarken = 4;
constexpr std::int64_t kSiegeRollTime = 18000;
constexpr std::int32_t kSiegeChance = 10;
constexpr std::int32_t kSiegeCentreAttempts = 10;
constexpr float kSiegeCentreDistance = 32.0F;
constexpr std::int32_t kSiegePositionAttempts = 10;
constexpr std::int32_t kSiegePositionRadius = 8;
constexpr std::int32_t kSiegePositionDiameter = 16;
constexpr std::int32_t kSiegeVillageSectionDistance = 1;
constexpr std::int32_t kSiegeZombieCount = 20;
constexpr std::int32_t kSiegeSpawnDelay = 2;
constexpr std::int32_t kSiegeMaxRandomSkyLight = 32;
constexpr std::int32_t kSiegeMaxMonsterBlockLight = 0;
constexpr std::int32_t kSiegeMonsterLightRange = 8;
constexpr std::int32_t kSiegeThunderSkyDampen = 10;
constexpr std::int32_t kSiegeMaxGroundEmission = 14;

/// parity: `Mth.sin` reads a 65,536-entry table, and the siege's ring of positions depends on the
/// exact values that table holds rather than on `std::sin` of the same angle.
constexpr double kSinIndexScale = 10430.378350470453;
constexpr std::int32_t kSinIndexMask = 65535;
constexpr std::int32_t kCosIndexOffset = 16384;

[[nodiscard]] float minecraftSin(float angle) {
    const auto index =
        static_cast<std::int32_t>(static_cast<std::int64_t>(static_cast<double>(angle) * kSinIndexScale)) &
        kSinIndexMask;
    return static_cast<float>(std::sin(index / kSinIndexScale));
}

[[nodiscard]] float minecraftCos(float angle) {
    const auto index =
        (static_cast<std::int32_t>(static_cast<std::int64_t>(static_cast<double>(angle) * kSinIndexScale)) +
         kCosIndexOffset) &
        kSinIndexMask;
    return static_cast<float>(std::sin(index / kSinIndexScale));
}

/// parity: `Monster.checkMonsterSpawnRules(EVENT)`, including its RNG order.
[[nodiscard]] bool checkZombieSpawnRules(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                         random::RandomSource& random) {
    if (worldLight::skyLight(world.state(), x, y, z) > random.nextInt(kSiegeMaxRandomSkyLight)) {
        return false;
    }
    if (worldLight::blockLight(world.state(), x, y, z) > kSiegeMaxMonsterBlockLight) {
        return false;
    }
    const std::int32_t skyDampen = world.weather().thundering
                                       ? kSiegeThunderSkyDampen
                                       : daylightDetectors::skyDarken(world.clock(), world.weather());
    if (worldLight::rawBrightness(world.state(), x, y, z, true, skyDampen) > random.nextInt(kSiegeMonsterLightRange)) {
        return false;
    }
    const std::optional<std::int32_t> ground = world.state().blockState(x, y - 1, z);
    return ground.has_value() && reg::blockShapes::isFaceSturdy(*ground, reg::BlockFace::Up) &&
           server::world::light::blockLight::emission(*ground) < kSiegeMaxGroundEmission;
}

[[nodiscard]] std::optional<VillageSiegeSpawnRequest>
findRandomSiegePos(LoadedWorld& world, std::int32_t centreX, std::int32_t centreZ, random::RandomSource& random) {
    for (std::int32_t attempt = 0; attempt < kSiegePositionAttempts; attempt++) {
        const std::int32_t x = centreX + random.nextInt(kSiegePositionDiameter) - kSiegePositionRadius;
        const std::int32_t z = centreZ + random.nextInt(kSiegePositionDiameter) - kSiegePositionRadius;
        const server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
        if (chunk == nullptr) {
            continue;
        }
        const std::int32_t y = chunk->heightAt(x & 15, z & 15, server::world::HeightmapType::WorldSurface);
        if (!villageProximity::isCloseToVillage(world.state(), x, y, z, kSiegeVillageSectionDistance) ||
            !checkZombieSpawnRules(world, x, y, z, random)) {
            continue;
        }
        return VillageSiegeSpawnRequest{.x = x + 0.5, .y = static_cast<double>(y), .z = z + 0.5};
    }
    return std::nullopt;
}

} // namespace

bool VillageSiegeSpawner::trySetUpSiege(LoadedWorld& world, const std::vector<player::PlayerSession*>& players,
                                        random::RandomSource& random) {
    for (const player::PlayerSession* player : players) {
        if (player->gamemode == wire::protocol::play::Gamemode::Spectator) {
            continue;
        }
        const std::int32_t centreX = blockOf(player->x);
        const std::int32_t centreY = blockOf(player->y);
        const std::int32_t centreZ = blockOf(player->z);
        if (!villageProximity::isCloseToVillage(world.state(), centreX, centreY, centreZ,
                                                kSiegeVillageSectionDistance)) {
            continue;
        }
        const std::optional<Identifier> biome = naturalSpawner::biomeAt(world, centreX, centreY, centreZ);
        if (biome.has_value() && biome->path() == "mushroom_fields") {
            continue;
        }
        for (std::int32_t attempt = 0; attempt < kSiegeCentreAttempts; attempt++) {
            const float angle = random.nextFloat() * static_cast<float>(std::numbers::pi * 2.0);
            spawnX_ = centreX + static_cast<std::int32_t>(std::floor(minecraftCos(angle) * kSiegeCentreDistance));
            spawnY_ = centreY;
            spawnZ_ = centreZ + static_cast<std::int32_t>(std::floor(minecraftSin(angle) * kSiegeCentreDistance));
            if (findRandomSiegePos(world, spawnX_, spawnZ_, random).has_value()) {
                nextSpawnTime_ = 0;
                zombiesToSpawn_ = kSiegeZombieCount;
                return true;
            }
        }
        // parity: a qualifying player is enough to count as set up, even when every probe failed.
        return true;
    }
    return false;
}

void VillageSiegeSpawner::tick(LoadedWorld& world, bool spawnEnemies,
                               const std::vector<player::PlayerSession*>& players, random::RandomSource& random,
                               const Spawn& spawn) {
    if (world.declaration().dimension != config::WorldDimension::Overworld) {
        return;
    }
    if (daylightDetectors::skyDarken(world.clock(), world.weather()) < kSiegeBrightOutsideDarken || !spawnEnemies) {
        siegeTonight_ = false;
        hasSetUpSiege_ = false;
        return;
    }
    const std::int64_t dayLength = WorldClock::kDayLength;
    if (((world.clock().totalTicks() % dayLength) + dayLength) % dayLength == kSiegeRollTime) {
        siegeTonight_ = random.nextInt(kSiegeChance) == 0;
    }
    if (!siegeTonight_) {
        return;
    }
    if (!hasSetUpSiege_) {
        if (!trySetUpSiege(world, players, random)) {
            return;
        }
        hasSetUpSiege_ = true;
    }
    if (nextSpawnTime_ > 0) {
        nextSpawnTime_--;
        return;
    }
    nextSpawnTime_ = kSiegeSpawnDelay;
    if (zombiesToSpawn_ <= 0) {
        siegeTonight_ = false;
        return;
    }
    if (const std::optional<VillageSiegeSpawnRequest> request = findRandomSiegePos(world, spawnX_, spawnZ_, random);
        request.has_value() && spawn) {
        spawn(*request);
    }
    zombiesToSpawn_--;
}

} // namespace basalt::server::runtime::world
