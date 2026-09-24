#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobLocalDifficulty.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/server/runtime/world/Precipitation.hpp>
#include <basalt/server/runtime/world/SkeletonHorseTrap.hpp>

#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::world::skeletonHorseTrap {
namespace {

namespace reg = server::registry;

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

/// parity: the spread of the shove the three extra horses get when the trap fires.
constexpr double kHorsePushSpread = 1.1485;

[[nodiscard]] std::int32_t blockOf(double coordinate) { return static_cast<std::int32_t>(std::floor(coordinate)); }

[[nodiscard]] const Identifier& skeletonHorse() {
    static const Identifier type = Identifier::vanilla("skeleton_horse");
    return type;
}

[[nodiscard]] const Identifier& skeleton() {
    static const Identifier type = Identifier::vanilla("skeleton");
    return type;
}

[[nodiscard]] NbtTag motion(double x, double y, double z) {
    return NbtTag::ofList(
        NbtList{wire::nbt::NbtType::Double, {NbtTag::ofDouble(x), NbtTag::ofDouble(y), NbtTag::ofDouble(z)}});
}

/// parity: the moon only exists where there is a clock, so the Nether contributes nothing.
[[nodiscard]] float moonBrightness(const LoadedWorld& world) {
    return config::hasClock(world.declaration().dimension)
               ? naturalSpawner::surfaceSlimeChance(world.clock().totalTicks()) * 2.0F
               : 0.0F;
}

MobSpawnContext contextOf(const SpawnEquipmentFinalizer& equipment, random::RandomSource& levelRandom,
                          float specialMultiplier, const std::optional<Identifier>& biome, std::int64_t gameTime,
                          std::string_view difficulty,
                          const std::function<random::RandomSource()>& entityRandomFactory) {
    MobSpawnContext context;
    context.random = &levelRandom;
    context.specialMultiplier = specialMultiplier;
    context.difficulty = std::string{difficulty};
    context.equipment = &equipment;
    context.biome = biome;
    context.gameTime = gameTime;
    context.reason = MobSpawnReason::Triggered;
    context.entityRandomFactory = entityRandomFactory;
    return context;
}

void finalizeNode(MobSpawnForestBuilder& builder, MobSpawnNode& node, const MobSpawnContext& context,
                  random::RandomSource& entityRandom) {
    MobSpawnGroupData group;
    mobSpawnFinalization::writeGeneratedNode(builder, node, 0, context, group, blockOf(node.x()), blockOf(node.y()),
                                             blockOf(node.z()), entityRandom);
}

MobSpawnNode& createSkeleton(MobSpawnForestBuilder& builder, double x, double y, double z,
                             const MobSpawnContext& context, const SpawnEquipmentFinalizer& equipment) {
    MobSpawnNode& rider = builder.create(skeleton(), x, y, z, 0.0F);
    random::RandomSource entityRandom = context.entityRandom();
    finalizeNode(builder, rider, context, entityRandom);
    rider.data().putBoolean("PersistenceRequired", true);
    equipment.finalizeSkeletonTrapEquipment(rider.data(), entityRandom, context.specialMultiplier);
    return rider;
}

} // namespace

bool shouldCreateFromThunder(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                             random::RandomSource& random) {
    if (!precipitation::rainingAt(world, x, y, z) || !world.gameRules().get(gameRules::kSpawnMobs)) {
        return false;
    }
    const server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return false;
    }
    const float effectiveDifficulty = mobLocalDifficulty::effectiveDifficulty(
        world.difficulty(), world.gameTime(), chunk->inhabitedTime(), moonBrightness(world));
    // The chance is before the rod test in vanilla, so a protected strike still consumes it.
    if (random.nextDouble() >= static_cast<double>(effectiveDifficulty) * kSpawnChanceScale) {
        return false;
    }
    const std::optional<std::int32_t> below = world.state().blockState(x, y - 1, z);
    if (!below.has_value()) {
        return true;
    }
    // parity: the test is against the tag `lightning_rods`, which is the eight stages of copper the
    // rod oxidises through and not the one block: an exposed rod protects the same.
    const Identifier* block = reg::blockStateDefinitions::blockOf(*below);
    return block == nullptr || !block->path().ends_with("lightning_rod");
}

MobSpawnForest buildActivationForest(entity::ServerEntity& horse, random::RandomSource& levelRandom,
                                     random::RandomSource& horseRandom,
                                     const std::function<random::RandomSource()>& entityRandomFactory,
                                     const SpawnEquipmentFinalizer& equipment, float specialMultiplier,
                                     const std::optional<Identifier>& biome, std::int64_t gameTime,
                                     std::string_view difficulty) {
    horse.skeletonTrap = false;
    horse.tamedHorse = true;
    horse.age = 0;

    const MobSpawnContext context =
        contextOf(equipment, levelRandom, specialMultiplier, biome, gameTime, difficulty, entityRandomFactory);
    MobSpawnForestBuilder builder;
    MobSpawnNode& originalRider = createSkeleton(builder, horse.x, horse.y, horse.z, context, equipment);
    // The trap horse already exists in the world, so its rider mounts it by id rather than by node.
    static_cast<void>(builder.mountExternal(originalRider, horse.entityId()));

    for (std::int32_t index = 0; index < 3; index++) {
        MobSpawnNode& otherHorse = builder.create(skeletonHorse(), horse.x, horse.y, horse.z, 0.0F);
        random::RandomSource horseEntityRandom = context.entityRandom();
        finalizeNode(builder, otherHorse, context, horseEntityRandom);
        otherHorse.data().putBoolean("PersistenceRequired", true);
        otherHorse.data().putBoolean("Tame", true);
        otherHorse.data().putInt("Age", 0);
        otherHorse.data().set("Motion", motion(horseRandom.triangle(0.0, kHorsePushSpread), 0.0,
                                               horseRandom.triangle(0.0, kHorsePushSpread)));

        MobSpawnNode& rider = createSkeleton(builder, horse.x, horse.y, horse.z, context, equipment);
        static_cast<void>(builder.mount(rider, otherHorse));
    }
    return builder.freeze(originalRider);
}

MobSpawnForest activate(LoadedWorld& world, entity::ServerEntity& horse, random::RandomSource& levelRandom,
                        random::RandomSource& horseRandom,
                        const std::function<random::RandomSource()>& entityRandomFactory,
                        const SpawnEquipmentFinalizer& equipment) {
    const std::int32_t blockX = blockOf(horse.x);
    const std::int32_t blockY = blockOf(horse.y);
    const std::int32_t blockZ = blockOf(horse.z);
    const server::world::Chunk* chunk = world.state().chunkIfLoaded(blockX >> 4, blockZ >> 4);
    const float specialMultiplier = mobLocalDifficulty::specialMultiplier(
        world.difficulty(), world.gameTime(), chunk == nullptr ? 0 : chunk->inhabitedTime(), moonBrightness(world));
    return buildActivationForest(horse, levelRandom, horseRandom, entityRandomFactory, equipment, specialMultiplier,
                                 naturalSpawner::biomeAt(world, blockX, blockY, blockZ), world.gameTime(),
                                 world.difficulty());
}

void tickLifetime(entity::ServerEntity& horse) {
    if (horse.persistent || !horse.skeletonTrap) {
        return;
    }
    const std::int32_t previous = horse.skeletonTrapTime++;
    if (previous >= kMaxLife) {
        horse.removed = true;
    }
}

} // namespace basalt::server::runtime::world::skeletonHorseTrap
