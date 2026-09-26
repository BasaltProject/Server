#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/server/runtime/world/SpawnPlacementRules.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/light/Light.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world::spawnPlacements {
namespace {

namespace reg = server::registry;

using wire::common::Identifier;

/// The predicate `SpawnPlacements` registers for a type.
enum class Rule : std::uint8_t {
    Mob,
    Monster,
    AnyLightMonster,
    SurfaceMonster,
    Animal,
    /// An animal rule over a block tag of its own, which never ignores the light.
    TaggedAnimal,
    Axolotl,
    SurfaceWaterAnimal,
    Nautilus,
    Drowned,
    Guardian,
    TropicalFish,
    Bat,
    InfestationMob,
    Ghast,
    GlowSquid,
    MagmaCube,
    Always,
    Ocelot,
    NotOnNetherWart,
    PatrollingMonster,
    PolarBear,
    SkeletonHorse,
    Slime,
    Stray,
    Strider,
    Turtle,
    ZombifiedPiglin,
};

struct Registration {
    std::string_view type;
    Rule rule;
    std::string_view tag;
};

/// parity: the `SpawnPlacements` static table, type by type.
constexpr auto kRegistrations = std::to_array<Registration>({
    {"axolotl", Rule::Axolotl, "minecraft:axolotls_spawnable_on"},
    {"cod", Rule::SurfaceWaterAnimal, {}},
    {"dolphin", Rule::SurfaceWaterAnimal, {}},
    {"drowned", Rule::Drowned, {}},
    {"guardian", Rule::Guardian, {}},
    {"pufferfish", Rule::SurfaceWaterAnimal, {}},
    {"salmon", Rule::SurfaceWaterAnimal, {}},
    {"squid", Rule::SurfaceWaterAnimal, {}},
    {"tropical_fish", Rule::TropicalFish, {}},
    {"armadillo", Rule::TaggedAnimal, "minecraft:armadillo_spawnable_on"},
    {"bat", Rule::Bat, "minecraft:bats_spawnable_on"},
    {"blaze", Rule::AnyLightMonster, {}},
    {"bogged", Rule::Monster, {}},
    {"breeze", Rule::AnyLightMonster, {}},
    {"camel", Rule::TaggedAnimal, "minecraft:camels_spawnable_on"},
    {"camel_husk", Rule::SurfaceMonster, {}},
    {"cave_spider", Rule::Monster, {}},
    {"chicken", Rule::Animal, {}},
    {"cow", Rule::Animal, {}},
    {"creeper", Rule::Monster, {}},
    {"donkey", Rule::Animal, {}},
    {"enderman", Rule::Monster, {}},
    {"endermite", Rule::InfestationMob, {}},
    {"ender_dragon", Rule::Mob, {}},
    {"frog", Rule::TaggedAnimal, "minecraft:frogs_spawnable_on"},
    {"ghast", Rule::Ghast, {}},
    {"happy_ghast", Rule::Animal, {}},
    {"giant", Rule::Monster, {}},
    {"glow_squid", Rule::GlowSquid, {}},
    {"goat", Rule::TaggedAnimal, "minecraft:goats_spawnable_on"},
    {"horse", Rule::Animal, {}},
    {"husk", Rule::SurfaceMonster, {}},
    {"iron_golem", Rule::Mob, {}},
    {"llama", Rule::Animal, {}},
    {"magma_cube", Rule::MagmaCube, {}},
    {"sulfur_cube", Rule::Always, {}},
    {"mooshroom", Rule::TaggedAnimal, "minecraft:mooshrooms_spawnable_on"},
    {"mule", Rule::Animal, {}},
    {"nautilus", Rule::Nautilus, {}},
    {"ocelot", Rule::Ocelot, {}},
    {"parrot", Rule::TaggedAnimal, "minecraft:parrots_spawnable_on"},
    {"pig", Rule::Animal, {}},
    {"hoglin", Rule::NotOnNetherWart, {}},
    {"piglin", Rule::NotOnNetherWart, {}},
    {"pillager", Rule::PatrollingMonster, {}},
    {"polar_bear", Rule::PolarBear, {}},
    {"rabbit", Rule::TaggedAnimal, "minecraft:rabbits_spawnable_on"},
    {"sheep", Rule::Animal, {}},
    {"silverfish", Rule::InfestationMob, {}},
    {"skeleton", Rule::Monster, {}},
    {"skeleton_horse", Rule::SkeletonHorse, {}},
    {"slime", Rule::Slime, {}},
    {"snow_golem", Rule::Mob, {}},
    {"spider", Rule::Monster, {}},
    {"stray", Rule::Stray, {}},
    {"parched", Rule::SurfaceMonster, {}},
    {"strider", Rule::Strider, {}},
    {"turtle", Rule::Turtle, {}},
    {"villager", Rule::Mob, {}},
    {"witch", Rule::Monster, {}},
    {"wither", Rule::Monster, {}},
    {"wither_skeleton", Rule::Monster, {}},
    {"wolf", Rule::TaggedAnimal, "minecraft:wolves_spawnable_on"},
    {"zoglin", Rule::AnyLightMonster, {}},
    {"creaking", Rule::Monster, {}},
    {"zombie", Rule::Monster, {}},
    {"zombie_horse", Rule::Monster, {}},
    {"zombified_piglin", Rule::ZombifiedPiglin, {}},
    {"zombie_villager", Rule::Monster, {}},
    {"cat", Rule::Animal, {}},
    {"elder_guardian", Rule::Guardian, {}},
    {"evoker", Rule::Monster, {}},
    {"fox", Rule::TaggedAnimal, "minecraft:foxes_spawnable_on"},
    {"illusioner", Rule::Monster, {}},
    {"panda", Rule::Animal, {}},
    {"phantom", Rule::Mob, {}},
    {"ravager", Rule::Monster, {}},
    {"shulker", Rule::Mob, {}},
    {"trader_llama", Rule::Animal, {}},
    {"vex", Rule::Monster, {}},
    {"vindicator", Rule::Monster, {}},
    {"wandering_trader", Rule::Mob, {}},
    {"warden", Rule::Monster, {}},
});

/// parity: the types built with `notInPeaceful`.
constexpr auto kNotInPeaceful = std::to_array<std::string_view>({"blaze",
                                                                 "bogged",
                                                                 "breeze",
                                                                 "cave_spider",
                                                                 "creaking",
                                                                 "creeper",
                                                                 "drowned",
                                                                 "elder_guardian",
                                                                 "enderman",
                                                                 "endermite",
                                                                 "evoker",
                                                                 "ghast",
                                                                 "giant",
                                                                 "guardian",
                                                                 "hoglin",
                                                                 "husk",
                                                                 "illusioner",
                                                                 "magma_cube",
                                                                 "parched",
                                                                 "phantom",
                                                                 "piglin_brute",
                                                                 "pillager",
                                                                 "ravager",
                                                                 "silverfish",
                                                                 "skeleton",
                                                                 "slime",
                                                                 "spider",
                                                                 "stray",
                                                                 "vex",
                                                                 "vindicator",
                                                                 "warden",
                                                                 "witch",
                                                                 "wither",
                                                                 "wither_skeleton",
                                                                 "zoglin",
                                                                 "zombie",
                                                                 "zombie_villager",
                                                                 "zombified_piglin"});

/// parity: `Animal.isBrightEnoughToSpawn`, raw light over eight.
constexpr std::int32_t kAnimalLight = 8;

/// parity: `Monster.isDarkEnoughToSpawn`'s thirty-two sky levels and its thunder darkening.
constexpr std::int32_t kSkyRollBound = 32;
constexpr std::int32_t kThunderSkyDarken = 10;
constexpr std::int32_t kNetherLightTest = 7;

/// parity: `PatrollingMonster.checkPatrollingMonsterSpawnRules`, block light above eight.
constexpr std::int32_t kPatrolBlockLight = 8;

[[nodiscard]] const Registration* registrationOf(const Identifier& type) {
    if (type.namespaceName() != Identifier::kVanillaNamespace) {
        return nullptr;
    }
    const auto found = std::ranges::find(kRegistrations, std::string_view{type.path()}, &Registration::type);
    return found == kRegistrations.end() ? nullptr : &*found;
}

[[nodiscard]] const config::DimensionFacts& factsOf(const LoadedWorld& world) {
    return config::dimensionFacts(world.declaration().dimension);
}

/// What the rules below ask of one block.
class Probe {
public:
    Probe(LoadedWorld& world, const SpawnRuleFacts& facts) : world_(world), facts_(facts) {}

    [[nodiscard]] const Identifier* blockAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
        const std::optional<std::int32_t> state = world_.state().blockState(x, y, z);
        return state.has_value() ? reg::blockStateDefinitions::blockOf(*state) : nullptr;
    }

    [[nodiscard]] bool isBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view path) const {
        const Identifier* block = blockAt(x, y, z);
        return block != nullptr && block->path() == path;
    }

    [[nodiscard]] bool isAir(std::int32_t x, std::int32_t y, std::int32_t z) const {
        return isBlock(x, y, z, "air") || isBlock(x, y, z, "cave_air") || isBlock(x, y, z, "void_air");
    }

    [[nodiscard]] bool isWaterFluid(std::int32_t x, std::int32_t y, std::int32_t z) const {
        const std::optional<std::int32_t> state = world_.state().blockState(x, y, z);
        return state.has_value() && reg::blockFluids::hasWaterFluid(*state);
    }

    [[nodiscard]] bool isLavaFluid(std::int32_t x, std::int32_t y, std::int32_t z) const {
        return isBlock(x, y, z, "lava");
    }

    [[nodiscard]] bool blockTagged(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view tag) const {
        const Identifier* block = blockAt(x, y, z);
        return block != nullptr && facts_.blockTagged && facts_.blockTagged(tag, *block);
    }

    [[nodiscard]] bool biomeTagged(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view tag) const {
        const std::optional<Identifier> biome = naturalSpawner::biomeAt(world_, x, y, z);
        return biome.has_value() && facts_.biomeTagged && facts_.biomeTagged(tag, *biome);
    }

    /// parity: `Animal.isBrightEnoughToSpawn`, `getRawBrightness(pos, 0) > 8`.
    [[nodiscard]] bool brightEnough(std::int32_t x, std::int32_t y, std::int32_t z) const {
        return worldLight::rawBrightness(world_.state(), x, y, z, factsOf(world_).hasSkylight) > kAnimalLight;
    }

    [[nodiscard]] bool peaceful() const { return world_.difficulty() == "peaceful"; }

    [[nodiscard]] std::int32_t seaLevel() const { return factsOf(world_).seaLevel; }

    [[nodiscard]] bool playerWithin(std::int32_t x, std::int32_t y, std::int32_t z, double range) const {
        return facts_.playerWithin && facts_.playerWithin(world_, x + 0.5, y + 0.5, z + 0.5, range);
    }

    [[nodiscard]] LoadedWorld& world() const { return world_; }

private:
    LoadedWorld& world_;
    const SpawnRuleFacts& facts_;
};

/// parity: `Mob.checkMobSpawnRules`. A spawner never asks the block below; anything else asks
/// `BlockState.isValidSpawn`, taken here as its default of a sturdy top and emission under fourteen.
[[nodiscard]] bool mobRules(const Probe& probe, MobSpawnReason reason, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (isSpawner(reason)) {
        return true;
    }
    const std::optional<std::int32_t> below = probe.world().state().blockState(x, y - 1, z);
    constexpr std::int32_t kMaxSpawnableEmission = 14;
    return below.has_value() && reg::blockShapes::isFaceSturdy(*below, reg::BlockFace::Up) &&
           server::world::light::blockLight::emission(*below) < kMaxSpawnableEmission;
}

/// parity: `Monster.checkMonsterSpawnRules`.
[[nodiscard]] bool monsterRules(const Probe& probe, MobSpawnReason reason, std::int32_t x, std::int32_t y,
                                std::int32_t z, random::RandomSource& random) {
    return (ignoresLightRequirements(reason) || isDarkEnoughToSpawn(probe.world(), x, y, z, random)) &&
           mobRules(probe, reason, x, y, z);
}

/// parity: `Animal.checkAnimalSpawnRules`.
[[nodiscard]] bool animalRules(const Probe& probe, MobSpawnReason reason, std::int32_t x, std::int32_t y,
                               std::int32_t z) {
    const bool bright = ignoresLightRequirements(reason) || probe.brightEnough(x, y, z);
    return probe.blockTagged(x, y - 1, z, "minecraft:animals_spawnable_on") && bright;
}

/// parity: `WaterAnimal.checkSurfaceWaterAnimalSpawnRules`, which the ageable water creatures copy.
[[nodiscard]] bool surfaceWaterRules(const Probe& probe, std::int32_t x, std::int32_t y, std::int32_t z) {
    constexpr std::int32_t kDepth = 13;
    const std::int32_t seaLevel = probe.seaLevel();
    return y >= seaLevel - kDepth && y <= seaLevel && probe.isWaterFluid(x, y - 1, z) &&
           probe.isBlock(x, y + 1, z, "water");
}

/// parity: `Drowned.checkDrownedSpawnRules`, whose frequency roll is drawn even when it cannot pass.
[[nodiscard]] bool drownedRules(const Probe& probe, MobSpawnReason reason, std::int32_t x, std::int32_t y,
                                std::int32_t z, random::RandomSource& random) {
    if (!probe.isWaterFluid(x, y - 1, z) && !isSpawner(reason)) {
        return false;
    }
    const bool canMonsterSpawn =
        !probe.peaceful() &&
        (ignoresLightRequirements(reason) || isDarkEnoughToSpawn(probe.world(), x, y, z, random)) &&
        (isSpawner(reason) || probe.isWaterFluid(x, y, z));
    if (canMonsterSpawn && (isSpawner(reason) || reason == MobSpawnReason::Reinforcement)) {
        return true;
    }
    constexpr std::int32_t kFrequentOdds = 15;
    constexpr std::int32_t kOdds = 40;
    constexpr std::int32_t kMinDepth = 5;
    if (probe.biomeTagged(x, y, z, "minecraft:more_frequent_drowned_spawns")) {
        return random.nextInt(kFrequentOdds) == 0 && canMonsterSpawn;
    }
    return random.nextInt(kOdds) == 0 && y < probe.seaLevel() - kMinDepth && canMonsterSpawn;
}

/// parity: `Guardian.checkGuardianSpawnRules`, in its own short-circuiting order.
[[nodiscard]] bool guardianRules(const Probe& probe, MobSpawnReason reason, std::int32_t x, std::int32_t y,
                                 std::int32_t z, random::RandomSource& random) {
    constexpr std::int32_t kOdds = 20;
    return (random.nextInt(kOdds) == 0 || !naturalSpawner::canSeeSkyFromBelowWater(probe.world(), x, y, z)) &&
           !probe.peaceful() && (isSpawner(reason) || probe.isWaterFluid(x, y, z)) && probe.isWaterFluid(x, y - 1, z);
}

/// parity: `Bat.checkBatSpawnRules`.
[[nodiscard]] bool batRules(const Probe& probe, MobSpawnReason reason, std::int32_t x, std::int32_t y, std::int32_t z,
                            random::RandomSource& random) {
    const server::world::Chunk* chunk = probe.world().state().chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr || y >= chunk->heightAt(x & 15, z & 15, server::world::HeightmapType::WorldSurface)) {
        return false;
    }
    constexpr std::int32_t kLightBound = 4;
    if (random.nextBoolean() || maxLocalRawBrightness(probe.world(), x, y, z) > random.nextInt(kLightBound)) {
        return false;
    }
    return probe.blockTagged(x, y - 1, z, "minecraft:bats_spawnable_on") && mobRules(probe, reason, x, y, z);
}

/// parity: `Slime.checkSlimeSpawnRules`: a spawner only asks the mob rules, anything else the
/// swamp surface and the slime chunks, each roll drawn in its own order.
[[nodiscard]] bool slimeRules(const Probe& probe, MobSpawnReason reason, std::int32_t x, std::int32_t y, std::int32_t z,
                              random::RandomSource& random) {
    if (probe.peaceful()) {
        return false;
    }
    if (isSpawner(reason)) {
        return mobRules(probe, reason, x, y, z);
    }
    constexpr std::int32_t kSurfaceMinY = 50;
    constexpr std::int32_t kSurfaceMaxY = 70;
    constexpr std::int32_t kSurfaceLightBound = 8;
    if (probe.biomeTagged(x, y, z, "minecraft:allows_surface_slime_spawns") && y > kSurfaceMinY && y < kSurfaceMaxY) {
        const float chance = naturalSpawner::surfaceSlimeChance(probe.world().clock().totalTicks());
        if (random.nextFloat() < chance &&
            maxLocalRawBrightness(probe.world(), x, y, z) <= random.nextInt(kSurfaceLightBound)) {
            return mobRules(probe, reason, x, y, z);
        }
    }
    constexpr std::int32_t kChunkOdds = 10;
    constexpr std::int32_t kChunkMaxY = 40;
    const bool slimeChunk = naturalSpawner::isSlimeChunk(probe.world().metadata().seed, x >> 4, z >> 4);
    if (random.nextInt(kChunkOdds) == 0 && slimeChunk && y < kChunkMaxY) {
        return mobRules(probe, reason, x, y, z);
    }
    return false;
}

/// parity: `Stray.checkStraySpawnRules`: the powder snow scan runs before anything else.
[[nodiscard]] bool strayRules(const Probe& probe, MobSpawnReason reason, std::int32_t x, std::int32_t y, std::int32_t z,
                              random::RandomSource& random) {
    std::int32_t skyY = y + 1;
    while (probe.isBlock(x, skyY, z, "powder_snow")) {
        ++skyY;
    }
    return monsterRules(probe, reason, x, y, z, random) &&
           (isSpawner(reason) || naturalSpawner::canSeeSky(probe.world(), x, skyY - 1, z));
}

/// parity: `Strider.checkStriderSpawnRules`: up through the lava, then air.
[[nodiscard]] bool striderRules(const Probe& probe, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::int32_t scanY = y + 1;
    const std::int32_t maxY = factsOf(probe.world()).chunkHeight.maxY();
    while (scanY <= maxY && probe.isLavaFluid(x, scanY, z)) {
        ++scanY;
    }
    return probe.isAir(x, scanY, z);
}

[[nodiscard]] bool check(const Probe& probe, const Registration& registration, MobSpawnReason reason, std::int32_t x,
                         std::int32_t y, std::int32_t z, random::RandomSource& random) {
    switch (registration.rule) {
    case Rule::Mob:
    case Rule::AnyLightMonster:
        return mobRules(probe, reason, x, y, z);
    case Rule::Monster:
        return monsterRules(probe, reason, x, y, z, random);
    case Rule::SurfaceMonster:
        return monsterRules(probe, reason, x, y, z, random) &&
               (isSpawner(reason) || naturalSpawner::canSeeSky(probe.world(), x, y, z));
    case Rule::Animal:
        return animalRules(probe, reason, x, y, z);
    case Rule::TaggedAnimal:
        return probe.blockTagged(x, y - 1, z, registration.tag) && probe.brightEnough(x, y, z);
    case Rule::Axolotl:
        return probe.blockTagged(x, y - 1, z, registration.tag);
    case Rule::SurfaceWaterAnimal:
        return surfaceWaterRules(probe, x, y, z);
    case Rule::Nautilus: {
        constexpr std::int32_t kMaxDepth = 25;
        constexpr std::int32_t kMinDepth = 5;
        const std::int32_t seaLevel = probe.seaLevel();
        return y >= seaLevel - kMaxDepth && y <= seaLevel - kMinDepth && probe.isWaterFluid(x, y - 1, z) &&
               probe.isBlock(x, y + 1, z, "water");
    }
    case Rule::Drowned:
        return drownedRules(probe, reason, x, y, z, random);
    case Rule::Guardian:
        return guardianRules(probe, reason, x, y, z, random);
    case Rule::TropicalFish:
        return probe.isWaterFluid(x, y - 1, z) && probe.isBlock(x, y + 1, z, "water") &&
               (probe.biomeTagged(x, y, z, "minecraft:allows_tropical_fish_spawns_at_any_height") ||
                surfaceWaterRules(probe, x, y, z));
    case Rule::Bat:
        return batRules(probe, reason, x, y, z, random);
    case Rule::InfestationMob: {
        // parity: `Endermite` and `Silverfish` alike: no player within five blocks unless a spawner.
        constexpr double kPlayerRange = 5.0;
        return mobRules(probe, reason, x, y, z) && (isSpawner(reason) || !probe.playerWithin(x, y, z, kPlayerRange));
    }
    case Rule::Ghast: {
        constexpr std::int32_t kOdds = 20;
        return !probe.peaceful() && random.nextInt(kOdds) == 0 && mobRules(probe, reason, x, y, z);
    }
    case Rule::GlowSquid: {
        constexpr std::int32_t kDepth = 33;
        return y <= probe.seaLevel() - kDepth &&
               worldLight::rawBrightness(probe.world().state(), x, y, z, factsOf(probe.world()).hasSkylight) == 0 &&
               probe.isBlock(x, y, z, "water");
    }
    case Rule::MagmaCube:
        return !probe.peaceful();
    case Rule::Always:
        return true;
    case Rule::Ocelot: {
        constexpr std::int32_t kOdds = 3;
        return random.nextInt(kOdds) != 0;
    }
    case Rule::NotOnNetherWart:
        return !probe.isBlock(x, y - 1, z, "nether_wart_block");
    case Rule::PatrollingMonster:
        return blockBrightness(probe.world(), x, y, z) <= kPatrolBlockLight && mobRules(probe, reason, x, y, z);
    case Rule::PolarBear:
        if (!probe.biomeTagged(x, y, z, "minecraft:polar_bears_spawn_on_alternate_blocks")) {
            return animalRules(probe, reason, x, y, z);
        }
        return probe.brightEnough(x, y, z) &&
               probe.blockTagged(x, y - 1, z, "minecraft:polar_bears_spawnable_on_alternate");
    case Rule::SkeletonHorse:
        if (!isSpawner(reason)) {
            return animalRules(probe, reason, x, y, z);
        }
        return ignoresLightRequirements(reason) || probe.brightEnough(x, y, z);
    case Rule::Slime:
        return slimeRules(probe, reason, x, y, z, random);
    case Rule::Stray:
        return strayRules(probe, reason, x, y, z, random);
    case Rule::Strider:
        return striderRules(probe, x, y, z);
    case Rule::Turtle: {
        constexpr std::int32_t kAboveSea = 4;
        return y < probe.seaLevel() + kAboveSea && probe.blockTagged(x, y - 1, z, "minecraft:sand") &&
               probe.brightEnough(x, y, z);
    }
    case Rule::ZombifiedPiglin:
        return !probe.peaceful() && !probe.isBlock(x, y - 1, z, "nether_wart_block");
    }
    return true;
}

} // namespace

bool isSpawner(MobSpawnReason reason) noexcept {
    return reason == MobSpawnReason::Spawner || reason == MobSpawnReason::TrialSpawner;
}

bool ignoresLightRequirements(MobSpawnReason reason) noexcept { return reason == MobSpawnReason::TrialSpawner; }

bool isAllowedInPeaceful(const Identifier& type) {
    return type.namespaceName() != Identifier::kVanillaNamespace ||
           std::ranges::find(kNotInPeaceful, std::string_view{type.path()}) == kNotInPeaceful.end();
}

bool checkSpawnRules(LoadedWorld& world, const Identifier& type, MobSpawnReason reason, std::int32_t x, std::int32_t y,
                     std::int32_t z, random::RandomSource& random, const SpawnRuleFacts& facts) {
    if (!isAllowedInPeaceful(type) && world.difficulty() == "peaceful") {
        return false;
    }
    const Registration* registration = registrationOf(type);
    return registration == nullptr || check(Probe{world, facts}, *registration, reason, x, y, z, random);
}

bool isDarkEnoughToSpawn(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                         random::RandomSource& random) {
    const config::DimensionFacts& facts = factsOf(world);
    if (worldLight::skyLight(world.state(), x, y, z, facts.hasSkylight) > random.nextInt(kSkyRollBound)) {
        return false;
    }
    // parity: `DimensionType.MonsterSettings`: the overworld allows no block light and rolls zero to
    // seven, the Nether ignores block light and tests against seven, the End allows none and fifteen.
    std::int32_t blockLightLimit = 0;
    std::optional<std::int32_t> fixedLightTest;
    switch (world.declaration().dimension) {
    case config::WorldDimension::Nether:
        blockLightLimit = worldLight::kMaxLevel;
        fixedLightTest = kNetherLightTest;
        break;
    case config::WorldDimension::End:
        fixedLightTest = worldLight::kMaxLevel;
        break;
    case config::WorldDimension::Overworld:
        break;
    }
    if (blockLightLimit < worldLight::kMaxLevel && worldLight::blockLight(world.state(), x, y, z) > blockLightLimit) {
        return false;
    }
    // parity: `Level.isThundering`, which only a sky without a ceiling can be.
    const bool thundering = facts.hasSkylight && !facts.hasCeiling && world.weather().thundering;
    const std::int32_t brightness = worldLight::rawBrightness(world.state(), x, y, z, facts.hasSkylight,
                                                              thundering ? kThunderSkyDarken : world.skyDarken());
    constexpr std::int32_t kOverworldLightRange = 8;
    const std::int32_t test = fixedLightTest.has_value() ? *fixedLightTest : random.nextInt(kOverworldLightRange);
    return brightness <= test;
}

std::int32_t maxLocalRawBrightness(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return worldLight::rawBrightness(world.state(), x, y, z, factsOf(world).hasSkylight, world.skyDarken());
}

float lightLevelDependentMagicValue(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    // parity: `DimensionType.ambientLight`: nothing in the overworld, a tenth in the Nether and a
    // quarter in the End.
    float ambient = 0.0F;
    switch (world.declaration().dimension) {
    case config::WorldDimension::Nether:
        ambient = 0.1F;
        break;
    case config::WorldDimension::End:
        ambient = 0.25F;
        break;
    case config::WorldDimension::Overworld:
        break;
    }
    const float level = static_cast<float>(maxLocalRawBrightness(world, x, y, z)) / 15.0F;
    const float curved = level / (4.0F - (3.0F * level));
    return curved + (ambient * (1.0F - curved));
}

std::int32_t effectiveSkyBrightness(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return std::max(worldLight::skyLight(world.state(), x, y, z, factsOf(world).hasSkylight) - world.skyDarken(), 0);
}

std::int32_t blockBrightness(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return worldLight::blockLight(world.state(), x, y, z);
}

} // namespace basalt::server::runtime::world::spawnPlacements
