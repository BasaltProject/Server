#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobSpawnChecks.hpp>
#include <basalt/server/runtime/world/NaturalSpawnObstruction.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world::mobSpawnChecks {
namespace {

namespace reg = server::registry;

using entity::Aabb;
using wire::common::Identifier;

/// parity: the `EntityTypes` constructors that are a plain `Mob` and not a `PathfinderMob`.
constexpr auto kPlainMobs = std::to_array<std::string_view>({"bat", "ender_dragon", "ghast", "phantom"});

/// parity: the `EntityTypes` constructors that extend `Monster`.
constexpr auto kMonsters = std::to_array<std::string_view>(
    {"blaze",    "bogged",         "breeze",          "cave_spider",     "creaking", "creeper",
     "drowned",  "elder_guardian", "enderman",        "endermite",       "evoker",   "giant",
     "guardian", "husk",           "illusioner",      "parched",         "piglin",   "piglin_brute",
     "pillager", "ravager",        "silverfish",      "skeleton",        "spider",   "stray",
     "vex",      "vindicator",     "warden",          "witch",           "wither",   "wither_skeleton",
     "zoglin",   "zombie",         "zombie_villager", "zombified_piglin"});

/// parity: the `EntityTypes` constructors that extend `Animal`.
constexpr auto kAnimals =
    std::to_array<std::string_view>({"armadillo", "axolotl",        "bee",          "camel",          "camel_husk",
                                     "cat",       "chicken",        "cow",          "donkey",         "fox",
                                     "frog",      "happy_ghast",    "goat",         "hoglin",         "horse",
                                     "llama",     "mooshroom",      "mule",         "nautilus",       "ocelot",
                                     "panda",     "parrot",         "pig",          "polar_bear",     "rabbit",
                                     "sheep",     "skeleton_horse", "sniffer",      "strider",        "trader_llama",
                                     "turtle",    "wolf",           "zombie_horse", "zombie_nautilus"});

/// parity: the other `PathfinderMob` constructors: golems, fish, villagers and cube mobs.
constexpr auto kPathfinders =
    std::to_array<std::string_view>({"allay", "cod", "copper_golem", "dolphin", "glow_squid", "iron_golem",
                                     "magma_cube", "pufferfish", "salmon", "shulker", "slime", "snow_golem", "squid",
                                     "sulfur_cube", "tadpole", "tropical_fish", "villager", "wandering_trader"});

/// parity: the `checkSpawnObstruction` overrides that only ask `isUnobstructed`.
constexpr auto kUnobstructedOnly = std::to_array<std::string_view>(
    {"axolotl", "cod", "dolphin", "drowned", "elder_guardian", "glow_squid", "guardian", "nautilus", "pufferfish",
     "salmon", "squid", "strider", "tadpole", "tropical_fish", "zombie_nautilus"});

/// parity: `InfestedBlock.isCompatibleHostBlock`.
constexpr auto kInfestationHosts =
    std::to_array<std::string_view>({"stone", "cobblestone", "stone_bricks", "mossy_stone_bricks",
                                     "cracked_stone_bricks", "chiseled_stone_bricks", "deepslate"});

/// parity: what a walk target value is when a mob likes where it is.
constexpr float kPreferred = 10.0F;
constexpr float kLightMidpoint = 0.5F;

/// parity: `BlockCollisions`.
constexpr double kCollisionEpsilon = 1.0E-7;

template <std::size_t N>
[[nodiscard]] bool holds(const std::array<std::string_view, N>& list, std::string_view path) {
    return std::ranges::find(list, path) != list.end();
}

[[nodiscard]] std::string_view blockPathAt(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    const Identifier* block = state.has_value() ? reg::blockStateDefinitions::blockOf(*state) : nullptr;
    return block == nullptr ? std::string_view{} : std::string_view{block->path()};
}

[[nodiscard]] bool hasWaterAt(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    return state.has_value() && reg::blockFluids::hasWaterFluid(*state);
}

[[nodiscard]] bool tagged(const SpawnRuleFacts& facts, std::string_view tag, LoadedWorld& world, std::int32_t x,
                          std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    const Identifier* block = state.has_value() ? reg::blockStateDefinitions::blockOf(*state) : nullptr;
    return block != nullptr && facts.blockTagged && facts.blockTagged(tag, *block);
}

/// parity: `LevelReader.getPathfindingCostFromLightLevels`.
[[nodiscard]] float lightCost(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return spawnPlacements::lightLevelDependentMagicValue(world, x, y, z) - kLightMidpoint;
}

/// parity: `Monster.getWalkTargetValue` and the monsters that override it.
[[nodiscard]] float monsterWalkTarget(LoadedWorld& world, std::string_view path, std::int32_t x, std::int32_t y,
                                      std::int32_t z) {
    if (path == "enderman" || path == "warden" || path == "pillager" || path == "creaking") {
        return 0.0F;
    }
    if (path == "giant") {
        return lightCost(world, x, y, z);
    }
    if (path == "silverfish" && holds(kInfestationHosts, blockPathAt(world, x, y - 1, z))) {
        return kPreferred;
    }
    if ((path == "guardian" || path == "elder_guardian") && hasWaterAt(world, x, y, z)) {
        return kPreferred + lightCost(world, x, y, z);
    }
    return -lightCost(world, x, y, z);
}

/// parity: `Animal.getWalkTargetValue` and the animals that override it.
[[nodiscard]] float animalWalkTarget(LoadedWorld& world, std::string_view path, std::int32_t x, std::int32_t y,
                                     std::int32_t z, const SpawnRuleFacts& facts) {
    if (path == "axolotl" || path == "nautilus" || path == "zombie_nautilus" || path == "happy_ghast") {
        // parity: `HappyGhast` answers zero, five or ten, never less.
        return 0.0F;
    }
    if (path == "bee") {
        const std::string_view here = blockPathAt(world, x, y, z);
        return here == "air" || here == "cave_air" || here == "void_air" ? kPreferred : 0.0F;
    }
    if (path == "hoglin") {
        return blockPathAt(world, x, y - 1, z) == "crimson_nylium" ? kPreferred : 0.0F;
    }
    if (path == "strider") {
        return blockPathAt(world, x, y, z) == "lava" ? kPreferred : 0.0F;
    }
    if (path == "mooshroom") {
        return blockPathAt(world, x, y - 1, z) == "mycelium" ? kPreferred : lightCost(world, x, y, z);
    }
    if (path == "turtle" && (hasWaterAt(world, x, y, z) || tagged(facts, "minecraft:sand", world, x, y - 1, z))) {
        return kPreferred;
    }
    if (path != "turtle" && blockPathAt(world, x, y - 1, z) == "grass_block") {
        return kPreferred;
    }
    return lightCost(world, x, y, z);
}

/// parity: `IronGolem.checkSpawnObstruction`.
[[nodiscard]] bool ironGolemFits(LoadedWorld& world, const Identifier& type, const Aabb& box, double x, double y,
                                 double z, std::span<const Aabb> players) {
    const auto blockX = static_cast<std::int32_t>(std::floor(x));
    const auto blockY = static_cast<std::int32_t>(std::floor(y));
    const auto blockZ = static_cast<std::int32_t>(std::floor(z));
    const std::optional<std::int32_t> below = world.state().blockState(blockX, blockY - 1, blockZ);
    if (!below.has_value() || !reg::blockShapes::isCollisionFaceFull(*below, reg::BlockFace::Up)) {
        return false;
    }
    for (std::int32_t above = 1; above < 3; ++above) {
        if (!naturalSpawner::isValidEmptySpawnBlock(world, type, blockX, blockY + above, blockZ)) {
            return false;
        }
    }
    return naturalSpawner::isValidEmptySpawnBlock(world, type, blockX, blockY, blockZ, false) &&
           isUnobstructed(world, box, players);
}

/// parity: `Ocelot.checkSpawnObstruction`: above the sea, on grass or leaves.
[[nodiscard]] bool ocelotFits(LoadedWorld& world, const Aabb& box, double x, double y, double z,
                              std::span<const Aabb> players, const SpawnRuleFacts& facts) {
    if (!isUnobstructed(world, box, players) || containsAnyLiquid(world, box)) {
        return false;
    }
    const auto blockX = static_cast<std::int32_t>(std::floor(x));
    const auto blockY = static_cast<std::int32_t>(std::floor(y));
    const auto blockZ = static_cast<std::int32_t>(std::floor(z));
    if (blockY < config::dimensionFacts(world.declaration().dimension).seaLevel) {
        return false;
    }
    return blockPathAt(world, blockX, blockY - 1, blockZ) == "grass_block" ||
           tagged(facts, "minecraft:leaves", world, blockX, blockY - 1, blockZ);
}

[[nodiscard]] Aabb boxOf(const entity::ServerEntity& other) {
    return Aabb::of(other.x, other.y, other.z, other.width(), other.height());
}

} // namespace

MobFamily familyOf(const Identifier& type) {
    if (type.namespaceName() != Identifier::kVanillaNamespace) {
        return MobFamily::NotMob;
    }
    const std::string_view path = type.path();
    if (holds(kMonsters, path)) {
        return MobFamily::Monster;
    }
    if (holds(kAnimals, path)) {
        return MobFamily::Animal;
    }
    if (holds(kPathfinders, path)) {
        return MobFamily::Pathfinder;
    }
    return holds(kPlainMobs, path) ? MobFamily::Mob : MobFamily::NotMob;
}

bool checkSpawnRules(LoadedWorld& world, const Identifier& type, std::int32_t x, std::int32_t y, std::int32_t z,
                     const SpawnRuleFacts& facts) {
    // parity: `PathfinderMob.checkSpawnRules`, `getWalkTargetValue(blockPosition()) >= 0`.
    switch (familyOf(type)) {
    case MobFamily::Monster:
        return monsterWalkTarget(world, type.path(), x, y, z) >= 0.0F;
    case MobFamily::Animal:
        return animalWalkTarget(world, type.path(), x, y, z, facts) >= 0.0F;
    case MobFamily::NotMob:
    case MobFamily::Mob:
    case MobFamily::Pathfinder:
        break;
    }
    return true;
}

bool checkSpawnObstruction(LoadedWorld& world, const Identifier& type, const Aabb& box, double x, double y, double z,
                           std::span<const Aabb> players, const SpawnRuleFacts& facts) {
    const std::string_view path = type.path();
    if (holds(kUnobstructedOnly, path)) {
        return isUnobstructed(world, box, players);
    }
    if (path == "ravager") {
        return !containsAnyLiquid(world, box);
    }
    if (path == "iron_golem") {
        return ironGolemFits(world, type, box, x, y, z, players);
    }
    if (path == "ocelot") {
        return ocelotFits(world, box, x, y, z, players, facts);
    }
    const bool fits = !containsAnyLiquid(world, box) && isUnobstructed(world, box, players);
    // parity: `Warden.checkSpawnObstruction` also wants its own box free of collisions.
    if (path == "warden") {
        return fits && noCollision(world, naturalSpawnObstruction::entityBox(type, x, y, z));
    }
    return fits;
}

bool noCollision(LoadedWorld& world, const Aabb& box) {
    const auto low = [](double value) { return static_cast<std::int32_t>(std::floor(value - kCollisionEpsilon)) - 1; };
    const auto high = [](double value) { return static_cast<std::int32_t>(std::floor(value + kCollisionEpsilon)) + 1; };
    for (std::int32_t bx = low(box.minX); bx <= high(box.maxX); ++bx) {
        for (std::int32_t by = low(box.minY); by <= high(box.maxY); ++by) {
            for (std::int32_t bz = low(box.minZ); bz <= high(box.maxZ); ++bz) {
                const std::optional<std::int32_t> state = world.state().blockState(bx, by, bz);
                if (!state.has_value() || reg::blockShapes::isEmptyShape(*state)) {
                    continue;
                }
                const std::span<const std::int32_t> shapes = reg::blockShapes::collisionBoxes(*state);
                constexpr double kUnits = reg::blockShapes::kUnits;
                for (std::size_t at = 0; at + 5 < shapes.size();
                     at += static_cast<std::size_t>(reg::blockShapes::kCorners)) {
                    const Aabb shape{.minX = bx + (shapes[at] / kUnits),
                                     .minY = by + (shapes[at + 1] / kUnits),
                                     .minZ = bz + (shapes[at + 2] / kUnits),
                                     .maxX = bx + (shapes[at + 3] / kUnits),
                                     .maxY = by + (shapes[at + 4] / kUnits),
                                     .maxZ = bz + (shapes[at + 5] / kUnits)};
                    if (shape.intersects(box)) {
                        return false;
                    }
                }
            }
        }
    }
    const Aabb reach = box.inflate(kCollisionEpsilon);
    for (const entity::ServerEntity* other : world.state().entities()) {
        if (!other->removed && naturalSpawnObstruction::canBeCollidedWith(other->type()) &&
            boxOf(*other).intersects(reach)) {
            return false;
        }
    }
    return true;
}

bool containsAnyLiquid(LoadedWorld& world, const Aabb& box) {
    const auto minX = static_cast<std::int32_t>(std::floor(box.minX));
    const auto minY = static_cast<std::int32_t>(std::floor(box.minY));
    const auto minZ = static_cast<std::int32_t>(std::floor(box.minZ));
    const auto maxX = static_cast<std::int32_t>(std::floor(box.maxX));
    const auto maxY = static_cast<std::int32_t>(std::floor(box.maxY));
    const auto maxZ = static_cast<std::int32_t>(std::floor(box.maxZ));
    for (std::int32_t bx = minX; bx <= maxX; ++bx) {
        for (std::int32_t by = minY; by <= maxY; ++by) {
            for (std::int32_t bz = minZ; bz <= maxZ; ++bz) {
                const std::optional<std::int32_t> state = world.state().blockState(bx, by, bz);
                if (state.has_value() && reg::blockFluids::hasFluid(*state)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool isUnobstructed(LoadedWorld& world, const Aabb& box, std::span<const Aabb> players) {
    for (const entity::ServerEntity* other : world.state().entities()) {
        if (!other->removed && naturalSpawnObstruction::blocksBuilding(other->type()) &&
            boxOf(*other).intersects(box)) {
            return false;
        }
    }
    return std::ranges::none_of(players, [&box](const Aabb& player) { return player.intersects(box); });
}

} // namespace basalt::server::runtime::world::mobSpawnChecks
