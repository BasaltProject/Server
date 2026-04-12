#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NaturalSpawnObstruction.hpp>
#include <basalt/server/runtime/world/NaturalSpawnPredicates.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>

namespace basalt::server::runtime::world::naturalSpawner {
namespace {

namespace reg = server::registry;

using entity::MobCategory;
using wire::common::Identifier;

/// parity: the 24 blocks a mob is never spawned within, squared.
constexpr double kMinPlayerDistanceSquared = 576.0;

/// parity: `spawnCategoryForPosition`'s three tries and its six blocks of spread.
constexpr std::int32_t kAttempts = 3;
constexpr std::int32_t kSpread = 6;

/// parity: `Animal.isBrightEnoughToSpawn`, light above eight.
constexpr std::int32_t kAnimalLight = 8;

/// parity: `ServerChunkCache`'s interval for persistent creature spawning.
constexpr std::int64_t kPersistentSpawnInterval = 400;

constexpr std::int64_t kDayTicks = 24000;
constexpr std::int64_t kMoonPeriodTicks = kDayTicks * 8;
constexpr std::int64_t kSlimeChunkSalt = 987234911;

[[nodiscard]] const std::set<std::string, std::less<>>& batGround() {
    static const std::set<std::string, std::less<>> all{"stone", "granite", "diorite", "andesite", "tuff", "deepslate"};
    return all;
}

/// parity: `BlockTags.RAILS`, the whole of `PREVENT_MOB_SPAWNING_INSIDE`.
[[nodiscard]] const std::set<std::string, std::less<>>& rails() {
    static const std::set<std::string, std::less<>> all{"rail", "powered_rail", "detector_rail", "activator_rail"};
    return all;
}

/// parity: `NodeEvaluator.isBurningBlock`, minus the lit campfire, which needs its state.
[[nodiscard]] const std::set<std::string, std::less<>>& burningBlocks() {
    static const std::set<std::string, std::less<>> all{"fire", "soul_fire", "lava", "magma_block", "lava_cauldron"};
    return all;
}

/// parity: the rest of `EntityType.isBlockDangerous`, which no mob is immune to.
[[nodiscard]] const std::set<std::string, std::less<>>& hurtsOnContact() {
    static const std::set<std::string, std::less<>> all{"wither_rose", "sweet_berry_bush", "cactus", "powder_snow"};
    return all;
}

[[nodiscard]] const std::set<std::string, std::less<>>& waterTypes() {
    static const std::set<std::string, std::less<>> all{"axolotl",        "cod",        "dolphin",  "drowned",
                                                        "elder_guardian", "glow_squid", "guardian", "nautilus",
                                                        "pufferfish",     "salmon",     "squid",    "tropical_fish"};
    return all;
}

[[nodiscard]] const std::set<std::string, std::less<>>& monstersIgnoringLight() {
    static const std::set<std::string, std::less<>> all{
        "blaze", "breeze", "ghast", "hoglin", "magma_cube", "piglin", "sulfur_cube", "zoglin", "zombified_piglin"};
    return all;
}

[[nodiscard]] const std::set<std::string, std::less<>>& reducedWaterAmbientBiomes() {
    static const std::set<std::string, std::less<>> all{"river", "frozen_river"};
    return all;
}

[[nodiscard]] const std::set<std::string, std::less<>>& surfaceSlimeBiomes() {
    static const std::set<std::string, std::less<>> all{"swamp", "mangrove_swamp"};
    return all;
}

[[nodiscard]] std::string_view blockPathOf(std::int32_t stateId) {
    const Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string_view{} : std::string_view{block->path()};
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, std::string_view name) {
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(std::string{name});
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] bool isWaterBlock(std::int32_t stateId) { return blockPathOf(stateId) == "water"; }

[[nodiscard]] bool isWaterFluid(std::int32_t stateId) {
    return isWaterBlock(stateId) || propertyOf(stateId, "waterlogged") == "true";
}

[[nodiscard]] bool isLavaFluid(std::int32_t stateId) { return blockPathOf(stateId) == "lava"; }

/// parity: `EntityType.isBlockDangerous`, whose burning half a fire-immune mob walks through.
[[nodiscard]] bool isDangerous(std::int32_t stateId, std::string_view path, const Identifier* type) {
    if (type != nullptr && reg::entityTypeShapes::isImmuneTo(*type, path)) {
        return false;
    }
    const bool burning = burningBlocks().contains(path) || (path == "campfire" && propertyOf(stateId, "lit") == "true");
    const bool immune = type != nullptr && reg::entityTypeShapes::isFireImmune(*type);
    if (burning && !immune) {
        return true;
    }
    return hurtsOnContact().contains(path);
}

/// parity: `NaturalSpawner.isValidEmptySpawnBlock`, all four of its answers. `fluidCounts` is off
/// where the caller hands it `Fluids.EMPTY` instead of the block's own fluid.
[[nodiscard]] bool canStandIn(std::int32_t stateId, const Identifier* type, bool fluidCounts = true) {
    if (reg::blockShapes::isFullCube(stateId) || redstone::isSignalSource(stateId) ||
        (fluidCounts && reg::blockFluids::hasFluid(stateId))) {
        return false;
    }
    const std::string_view path = blockPathOf(stateId);
    if (path.empty() || rails().contains(path)) {
        return false;
    }
    return !isDangerous(stateId, path, type);
}

[[nodiscard]] bool canSeeSkyAbovePowderSnow(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::int32_t scanY = y + 1;
    while (true) {
        const std::optional<std::int32_t> state = world.state().blockState(x, scanY, z);
        if (!state.has_value()) {
            return false;
        }
        if (blockPathOf(*state) != "powder_snow") {
            break;
        }
        scanY++;
    }
    return canSeeSky(world, x, scanY - 1, z);
}

[[nodiscard]] bool canSpawnBat(LoadedWorld& world, std::int32_t ground, std::int32_t x, std::int32_t y, std::int32_t z,
                               random::RandomSource& random) {
    const server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr || y >= chunk->heightAt(x & 15, z & 15, server::world::HeightmapType::WorldSurface)) {
        return false;
    }
    if (random.nextBoolean()) {
        return false;
    }
    if (worldLight::rawBrightness(world.state(), x, y, z) > random.nextInt(4)) {
        return false;
    }
    return batGround().contains(blockPathOf(ground));
}

/// parity: 26.2 `Slime.checkSlimeSpawnRules`, including its moon timeline and slime chunks.
[[nodiscard]] bool canSpawnSlime(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                 random::RandomSource& random) {
    if (world.difficulty() == "peaceful") {
        return false;
    }
    const std::optional<Identifier> biome = biomeAt(world, x, y, z);
    const bool surfaceBiome = biome.has_value() && surfaceSlimeBiomes().contains(biome->path());
    if (surfaceBiome && y > 50 && y < 70) {
        if (random.nextFloat() < surfaceSlimeChance(world.clock().totalTicks()) &&
            worldLight::rawBrightness(world.state(), x, y, z, true, world.skyDarken()) <= random.nextInt(8)) {
            return true;
        }
    }
    const bool slimeChunk = isSlimeChunk(world.metadata().seed, x >> 4, z >> 4);
    return random.nextInt(10) == 0 && slimeChunk && y < 40;
}

/// parity: `SpawnPlacementTypes.IN_WATER` plus the common water-animal predicates.
[[nodiscard]] bool canSpawnInWater(LoadedWorld& world, std::string_view type, std::int32_t x, std::int32_t y,
                                   std::int32_t z, random::RandomSource& random) {
    const std::optional<std::int32_t> at = world.state().blockState(x, y, z);
    const std::optional<std::int32_t> above = world.state().blockState(x, y + 1, z);
    if (!at.has_value() || !above.has_value()) {
        return false;
    }
    if (!naturalSpawnPredicates::waterPlacementAllows(isWaterFluid(*at), reg::blockSupport::conductsRedstone(*above))) {
        return false;
    }
    const std::optional<std::int32_t> below = world.state().blockState(x, y - 1, z);
    if (!below.has_value()) {
        return false;
    }
    const std::int32_t seaLevel = config::dimensionFacts(world.declaration().dimension).seaLevel;
    if (type == "axolotl") {
        return blockPathOf(*below) == "clay";
    }
    if (type == "glow_squid") {
        return naturalSpawnPredicates::glowSquidAllows(
            y, seaLevel,
            std::max(worldLight::skyLight(world.state(), x, y, z), worldLight::blockLight(world.state(), x, y, z)),
            isWaterBlock(*at));
    }
    if (type == "guardian" || type == "elder_guardian") {
        return naturalSpawnPredicates::guardianAllows(
            random.nextInt(20), world.difficulty(), isWaterFluid(*at), isWaterFluid(*below),
            [&world, x, y, z] { return canSeeSkyFromBelowWater(world, x, y, z); });
    }
    if (type == "nautilus") {
        return y >= seaLevel - 25 && y <= seaLevel - 5 && isWaterFluid(*below) && isWaterBlock(*above);
    }
    if (type == "cod" || type == "dolphin" || type == "pufferfish" || type == "salmon" || type == "squid") {
        return y >= seaLevel - 13 && y <= seaLevel && isWaterFluid(*below) && isWaterBlock(*above);
    }
    if (type == "tropical_fish") {
        const std::optional<Identifier> biome = biomeAt(world, x, y, z);
        const bool anyHeight = biome.has_value() && biome->path() == "lush_caves";
        return isWaterFluid(*below) && isWaterBlock(*above) && (anyHeight || (y >= seaLevel - 13 && y <= seaLevel));
    }
    if (type == "drowned") {
        const bool dark = worldLight::skyLight(world.state(), x, y, z) <= random.nextInt(32) &&
                          worldLight::blockLight(world.state(), x, y, z) <= 0;
        const std::optional<Identifier> biome = biomeAt(world, x, y, z);
        const bool river = biome.has_value() && biome->path().find("river") != std::string::npos;
        const bool frequencyAllows = river ? random.nextInt(15) == 0 : y < seaLevel - 5 && random.nextInt(40) == 0;
        return dark && isWaterFluid(*below) && frequencyAllows;
    }
    return true;
}

/// parity: `SpawnPlacementTypes.IN_LAVA` and a strider's scan to the lava surface.
[[nodiscard]] bool canSpawnInLava(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> at = world.state().blockState(x, y, z);
    if (!at.has_value() || !isLavaFluid(*at)) {
        return false;
    }
    const std::int32_t maxY = config::dimensionFacts(world.declaration().dimension).chunkHeight.maxY();
    for (std::int32_t aboveY = y + 1; aboveY <= maxY; aboveY++) {
        const std::optional<std::int32_t> above = world.state().blockState(x, aboveY, z);
        if (!above.has_value()) {
            return false;
        }
        if (!isLavaFluid(*above)) {
            return blockPathOf(*above) == "air";
        }
    }
    return false;
}

[[nodiscard]] bool monsterTypeRule(std::string_view type, std::int32_t ground, LoadedWorld& world, std::int32_t x,
                                   std::int32_t y, std::int32_t z, random::RandomSource& random) {
    const std::string_view groundPath = blockPathOf(ground);
    if (type == "ghast") {
        return random.nextInt(20) == 0;
    }
    if (type == "hoglin" || type == "piglin" || type == "zombified_piglin") {
        return groundPath != "nether_wart_block";
    }
    if (type == "stray") {
        return canSeeSkyAbovePowderSnow(world, x, y, z);
    }
    return !naturalSpawnPredicates::isSurfaceMonster(type) || canSeeSky(world, x, y, z);
}

/// Whether the position leaves room for this type, blocks and standing entities alike.
[[nodiscard]] bool obstructionAllows(LoadedWorld& world, const Identifier& type, double x, double y, double z) {
    if (!naturalSpawnObstruction::blocksAllow(type, x, y, z,
                                              [&world](std::int32_t bx, std::int32_t by, std::int32_t bz) {
                                                  return world.state().blockState(bx, by, bz);
                                              })) {
        return false;
    }
    return !world.state().hasNaturalSpawnEntityObstruction(naturalSpawnObstruction::spawnBox(type, x, y, z),
                                                           naturalSpawnObstruction::entityBox(type, x, y, z));
}

} // namespace

std::int32_t capFor(MobCategory category, std::int32_t simulatedChunks) {
    return entity::mobCategoryFacts(category).maxPerChunk * simulatedChunks / kCapChunks;
}

std::vector<MobCategory> categoriesFor(std::string_view difficulty, std::int64_t gameTime) {
    static constexpr std::array<MobCategory, 7> kSpawning{MobCategory::Monster,
                                                          MobCategory::Creature,
                                                          MobCategory::Ambient,
                                                          MobCategory::Axolotls,
                                                          MobCategory::UndergroundWaterCreature,
                                                          MobCategory::WaterCreature,
                                                          MobCategory::WaterAmbient};
    std::vector<MobCategory> categories;
    for (const MobCategory category : kSpawning) {
        const entity::MobCategoryFacts& facts = entity::mobCategoryFacts(category);
        if ((difficulty == "peaceful" && !facts.friendly) ||
            (facts.persistent && gameTime % kPersistentSpawnInterval != 0)) {
            continue;
        }
        categories.push_back(category);
    }
    return categories;
}

std::int32_t spawnTopY(const server::world::Chunk& chunk, std::int32_t localX, std::int32_t localZ) {
    // parity: `NaturalSpawner` adds one to a heightmap that answers with the topmost solid block.
    // Ours answers with the free space over it, so it is already the same number.
    return chunk.heightAt(localX, localZ, server::world::HeightmapType::WorldSurface);
}

std::int32_t maxClusterSize(const Identifier& type) {
    const std::string& path = type.path();
    if (path == "ghast" || path == "happy_ghast" || path == "pillager") {
        return 1;
    }
    if (path == "horse" || path == "donkey" || path == "mule" || path == "skeleton_horse" || path == "zombie_horse") {
        return 6;
    }
    if (path == "cod" || path == "pufferfish" || path == "tropical_fish" || path == "wolf") {
        return 8;
    }
    return path == "salmon" ? 5 : 4;
}

bool canSpawnAtDistance(const Identifier& type, MobCategory fallbackCategory, double distanceSquared) {
    const MobCategory category =
        entity::mobCategoryByName(reg::entityTypeShapes::categoryOf(type)).value_or(fallbackCategory);
    const auto despawnDistance = static_cast<double>(entity::mobCategoryFacts(category).despawnDistance);
    return distanceSquared <= despawnDistance * despawnDistance || reg::entityTypeShapes::canSpawnFarFromPlayer(type);
}

bool containsSpawnData(const std::vector<SpawnerEntry>& entries, const SpawnerEntry& selected) {
    return std::ranges::any_of(entries, [&selected](const SpawnerEntry& candidate) {
        return candidate.type == selected.type && candidate.minCount == selected.minCount &&
               candidate.maxCount == selected.maxCount && candidate.constantCount == selected.constantCount;
    });
}

bool canSeeSkyFromBelowWater(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int32_t seaLevel = config::dimensionFacts(world.declaration().dimension).seaLevel;
    if (y >= seaLevel) {
        return canSeeSky(world, x, y, z);
    }
    if (!canSeeSky(world, x, seaLevel, z)) {
        return false;
    }
    for (std::int32_t scanY = seaLevel - 1; scanY > y; scanY--) {
        const std::optional<std::int32_t> state = world.state().blockState(x, scanY, z);
        if (!state.has_value()) {
            return false;
        }
        const std::string_view block = blockPathOf(*state);
        if (reg::blockShapes::dampening(*state) > 0 && block != "water" && block != "lava") {
            return false;
        }
    }
    return true;
}

bool canSeeSky(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return worldLight::skyLight(world.state(), x, y, z) >= worldLight::kMaxLevel;
}

float surfaceSlimeChance(std::int64_t clockTicks) {
    const std::int64_t wrapped = ((clockTicks % kMoonPeriodTicks) + kMoonPeriodTicks) % kMoonPeriodTicks;
    const auto phase = static_cast<std::int32_t>(wrapped / kDayTicks);
    return static_cast<float>(std::abs(4 - phase)) * 0.125F;
}

bool isSlimeChunk(std::int64_t worldSeed, std::int32_t chunkX, std::int32_t chunkZ) {
    // parity: the int products wrap, which is what Java does and what the seed depends on.
    const auto xSquare =
        static_cast<std::int32_t>(static_cast<std::uint32_t>(chunkX) * static_cast<std::uint32_t>(chunkX) * 4987142U);
    const auto xLinear = static_cast<std::int32_t>(static_cast<std::uint32_t>(chunkX) * 5947611U);
    const auto zSquare = static_cast<std::int64_t>(static_cast<std::int32_t>(static_cast<std::uint32_t>(chunkZ) *
                                                                             static_cast<std::uint32_t>(chunkZ))) *
                         4392871LL;
    const auto zLinear = static_cast<std::int32_t>(static_cast<std::uint32_t>(chunkZ) * 389711U);
    const std::int64_t seed = (worldSeed + xSquare + xLinear + zSquare + zLinear) ^ kSlimeChunkSalt;
    random::RandomSource random = random::RandomSource::legacy(seed);
    return random.nextInt(10) == 0;
}

const SpawnerEntry* pick(const std::vector<SpawnerEntry>& entries, random::RandomSource& random) {
    std::int32_t total = 0;
    for (const SpawnerEntry& entry : entries) {
        total += entry.weight;
    }
    if (total <= 0) {
        return nullptr;
    }
    std::int32_t roll = random.nextInt(total);
    for (const SpawnerEntry& entry : entries) {
        roll -= entry.weight;
        if (roll < 0) {
            return &entry;
        }
    }
    return entries.empty() ? nullptr : &entries.back();
}

std::int32_t packSize(const SpawnerEntry& entry, random::RandomSource& random) {
    if (entry.constantCount) {
        return entry.minCount;
    }
    return entry.minCount + random.nextInt(std::max(1, 1 + entry.maxCount - entry.minCount));
}

std::optional<Identifier> biomeAt(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const auto noiseBiomeAt = [&world](std::int32_t quartX, std::int32_t quartY,
                                       std::int32_t quartZ) -> std::optional<Identifier> {
        const server::world::Chunk* chunk = world.state().chunkIfLoaded(quartX >> 2, quartZ >> 2);
        if (chunk != nullptr) {
            const server::world::ChunkHeight bounds = chunk->verticalBounds();
            const std::int32_t localQuartY = std::clamp(quartY - (bounds.minY() >> 2), 0, bounds.height() / 4 - 1);
            const std::int32_t biomeId = chunk->biomeId(quartX & 3, localQuartY, quartZ & 3);
            if (const reg::Biome* biome = reg::biomes::registry().byId(biomeId); biome != nullptr) {
                return biome->id;
            }
        }
        if (world.biomeSource() != nullptr) {
            const std::expected<Identifier, wire::common::IdentifierError> parsed =
                Identifier::parse(world.biomeSource()->biomeAtQuart(quartX, quartY, quartZ));
            return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
        }
        return std::nullopt;
    };

    if (const std::optional<Identifier> zoomed = world.biomeZoomer().biomeAt(x, y, z, noiseBiomeAt);
        zoomed.has_value()) {
        return zoomed;
    }
    const server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
    if (chunk != nullptr) {
        const server::world::ChunkHeight bounds = chunk->verticalBounds();
        const std::int32_t clampedY = std::clamp(y, bounds.minY(), bounds.maxY());
        const std::int32_t quartY = (clampedY - bounds.minY()) >> 2;
        const std::int32_t biomeId = chunk->biomeId((x & 15) >> 2, quartY, (z & 15) >> 2);
        if (const reg::Biome* biome = reg::biomes::registry().byId(biomeId); biome != nullptr) {
            return biome->id;
        }
    }
    if (world.biomeSource() != nullptr) {
        const std::expected<Identifier, wire::common::IdentifierError> parsed =
            Identifier::parse(world.biomeSource()->blockBiomeAt(x, y, z));
        return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
    }
    return std::nullopt;
}

bool canSpawnAt(LoadedWorld& world, const Identifier* type, MobCategory category, std::int32_t x, std::int32_t y,
                std::int32_t z, random::RandomSource& random) {
    const std::string_view typePath = type == nullptr ? std::string_view{} : std::string_view{type->path()};
    if (typePath == "strider") {
        return canSpawnInLava(world, x, y, z);
    }
    if (!typePath.empty() && waterTypes().contains(typePath)) {
        return canSpawnInWater(world, typePath, x, y, z, random);
    }

    const std::optional<std::int32_t> ground = world.state().blockState(x, y - 1, z);
    if (!ground.has_value()) {
        return false;
    }
    if (naturalSpawnPredicates::usesGroundPlacement(typePath) &&
        !reg::blockShapes::isFaceSturdy(*ground, reg::BlockFace::Up)) {
        return false;
    }
    const std::optional<std::int32_t> feet = world.state().blockState(x, y, z);
    const std::optional<std::int32_t> head = world.state().blockState(x, y + 1, z);
    if (!feet.has_value() || !head.has_value() || !canStandIn(*feet, type) || !canStandIn(*head, type)) {
        return false;
    }
    const std::int32_t seaLevel = config::dimensionFacts(world.declaration().dimension).seaLevel;
    if (typePath == "ocelot") {
        const std::string_view groundPath = blockPathOf(*ground);
        return y >= seaLevel && (groundPath == "grass_block" || reg::blockStateFamilies::isLeavesState(*ground)) &&
               random.nextInt(3) != 0;
    }
    if (typePath == "bat") {
        return canSpawnBat(world, *ground, x, y, z, random);
    }
    if (typePath == "slime") {
        return canSpawnSlime(world, x, y, z, random);
    }

    if (category == MobCategory::Monster) {
        // parity: `Monster.isDarkEnoughToSpawn`: the sky has to be dark where it stands, and the
        // block light under the dimension's own limit, which is zero in the overworld.
        const bool dark = monstersIgnoringLight().contains(typePath) ||
                          (worldLight::skyLight(world.state(), x, y, z) <= random.nextInt(32) &&
                           worldLight::blockLight(world.state(), x, y, z) <= 0);
        return dark && monsterTypeRule(typePath, *ground, world, x, y, z, random);
    }
    if (category == MobCategory::Creature) {
        const std::string_view groundPath = blockPathOf(*ground);
        if (groundPath.empty()) {
            return false;
        }
        const std::optional<Identifier> biome = biomeAt(world, x, y, z);
        return naturalSpawnPredicates::allows(typePath, groundPath, reg::blockStateFamilies::isLeavesState(*ground),
                                              biome.has_value() ? std::string_view{biome->path()} : std::string_view{},
                                              y, seaLevel) &&
               worldLight::rawBrightness(world.state(), x, y, z) > kAnimalLight;
    }
    return true;
}

bool isValidEmptySpawnBlock(LoadedWorld& world, const Identifier& type, std::int32_t x, std::int32_t y, std::int32_t z,
                            bool fluidCounts) {
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    return state.has_value() && canStandIn(*state, &type, fluidCounts);
}

bool canSpawnOnGround(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> ground = world.state().blockState(x, y - 1, z);
    return ground.has_value() && reg::blockShapes::isFaceSturdy(*ground, reg::BlockFace::Up);
}

std::vector<SpawnRequest> spawnForChunk(LoadedWorld& world, std::int32_t chunkX, std::int32_t chunkZ,
                                        MobCategory category, const Hooks& hooks, random::RandomSource& random) {
    const server::world::Chunk* chunk = world.state().chunkIfLoaded(chunkX, chunkZ);
    if (chunk == nullptr || !hooks.entriesFor || !hooks.playerDistanceSquared) {
        return {};
    }
    const std::int32_t localX = random.nextInt(16);
    const std::int32_t localZ = random.nextInt(16);
    const std::int32_t topEmptyY = spawnTopY(*chunk, localX, localZ);
    const std::int32_t minY = chunk->verticalBounds().minY();
    if (topEmptyY <= minY) {
        return {};
    }
    // parity: `getRandomPosWithin` picks between the world's floor and the surface, and the floor is
    // below zero: taking the height as a bound crashed on every world whose surface is negative,
    // which is every superflat one.
    const std::int32_t startY = minY + random.nextInt(topEmptyY - minY + 1);
    const std::int32_t startX = (chunkX << 4) + localX;
    const std::int32_t startZ = (chunkZ << 4) + localZ;

    // parity: a position inside a block that conducts redstone is refused outright.
    const std::optional<std::int32_t> at = world.state().blockState(startX, startY, startZ);
    if (!at.has_value() || reg::blockSupport::conductsRedstone(*at)) {
        return {};
    }

    const std::array<double, 3>& spawn = world.spawn();
    std::vector<SpawnRequest> requests;
    std::int32_t clusterSize = 0;
    for (std::int32_t attempt = 0; attempt < kAttempts; attempt++) {
        std::int32_t x = startX;
        std::int32_t z = startZ;
        std::optional<SpawnerEntry> chosen;
        auto pack = static_cast<std::int32_t>(std::ceil(random.nextFloat() * 4.0F));
        std::int32_t spawned = 0;
        std::int32_t index = 0;
        std::optional<std::int32_t> sharedVariant;
        while (index < pack) {
            index++;
            x += random.nextInt(kSpread) - random.nextInt(kSpread);
            z += random.nextInt(kSpread) - random.nextInt(kSpread);
            const double centreX = x + 0.5;
            const double centreZ = z + 0.5;
            const std::optional<double> distance =
                hooks.playerDistanceSquared(centreX, static_cast<double>(startY), centreZ);
            if (!distance.has_value() || *distance <= kMinPlayerDistanceSquared) {
                continue;
            }
            const double spawnDx = spawn[0] - centreX;
            const double spawnDy = spawn[1] - startY;
            const double spawnDz = spawn[2] - centreZ;
            if (spawnDx * spawnDx + spawnDy * spawnDy + spawnDz * spawnDz <= kMinPlayerDistanceSquared) {
                continue;
            }
            if ((x >> 4) != chunkX || (z >> 4) != chunkZ) {
                const bool loaded = hooks.canSpawnEntitiesInChunk
                                        ? hooks.canSpawnEntitiesInChunk(x >> 4, z >> 4)
                                        : world.state().chunkIfLoaded(x >> 4, z >> 4) != nullptr;
                if (!loaded) {
                    continue;
                }
            }
            const std::optional<Identifier> biome = biomeAt(world, x, startY, z);
            if (!biome.has_value()) {
                continue;
            }
            if (!chosen.has_value()) {
                if (category == MobCategory::WaterAmbient && reducedWaterAmbientBiomes().contains(biome->path()) &&
                    random.nextFloat() < 0.98F) {
                    break;
                }
                const std::vector<SpawnerEntry> entries = hooks.entriesFor(*biome, x, startY, z);
                const SpawnerEntry* picked = pick(entries, random);
                if (picked == nullptr) {
                    break;
                }
                chosen = *picked;
                pack = packSize(*chosen, random);
            }
            if (!canSpawnAtDistance(chosen->type, category, *distance)) {
                continue;
            }
            if (!containsSpawnData(hooks.entriesFor(*biome, x, startY, z), *chosen)) {
                continue;
            }
            if (!canSpawnAt(world, &chosen->type, category, x, startY, z, random)) {
                continue;
            }
            if (!obstructionAllows(world, chosen->type, centreX, static_cast<double>(startY), centreZ)) {
                continue;
            }
            const SpawnRequest request{.type = chosen->type,
                                       .x = centreX,
                                       .y = static_cast<double>(startY),
                                       .z = centreZ,
                                       .yaw = random.nextFloat() * 360.0F,
                                       .groupIndex = spawned,
                                       .biome = biome};
            if (hooks.requestAccepted && !hooks.requestAccepted(request)) {
                continue;
            }
            requests.push_back(request);
            spawned++;
            clusterSize++;
            if (clusterSize >= maxClusterSize(chosen->type)) {
                return requests;
            }
            // parity: a shoal of tropical fish stops at the first member until its variant is
            // decided, which is what the caller's group data records.
            if (chosen->type.path() == "tropical_fish" && !sharedVariant.has_value()) {
                break;
            }
            if (spawned >= pack) {
                break;
            }
        }
    }
    return requests;
}

} // namespace basalt::server::runtime::world::naturalSpawner
