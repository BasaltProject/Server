#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/MobSpawnForest.hpp>
#include <basalt/server/runtime/world/SpawnEquipment.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::world {

/// World conditions consulted by `Cat.finalizeSpawn` after its superclass finalizer.
struct CatSpawnContext {
    float moonBrightness = 0.0F;
    bool inBlackCatStructure = false;
};

/// Mutable concrete `SpawnGroupData` shared by accepted members of one natural pack.
///
/// parity: what one pack agrees on, which is why the second sheep of a flock is the same colour as
/// the first and the third zombie of a horde is a baby if the first was.
struct MobSpawnGroupData {
    std::optional<std::int32_t> variant;
    std::optional<std::int32_t> secondaryVariant;
    std::optional<std::string> spiderEffect;
    std::optional<bool> zombieBaby;
    bool zombieCanSpawnJockey = true;
    bool patrolLeader = false;
    CatSpawnContext catSpawnContext;
};

/// Everything a finalizer needs besides the mob itself and the pack it belongs to.
///
/// A struct rather than fourteen arguments repeated down a recursion: every finalizer here calls at
/// least one other, and threading them by hand is where a wrong `reason` or a swapped random hides.
struct MobSpawnContext {
    /// The level's own stream, which is what the spawn attempt is drawing from.
    random::RandomSource* random = nullptr;
    /// parity: `specialMultiplier`, local difficulty: it scales the gear, the leaders and the size.
    float specialMultiplier = 0.0F;
    std::string difficulty = "easy";
    const SpawnEquipmentFinalizer* equipment = nullptr;
    std::optional<wire::common::Identifier> biome;
    std::int64_t gameTime = 0;
    MobSpawnReason reason = MobSpawnReason::Natural;
    /// A fresh per-entity stream, which vanilla seeds anew for each mob it constructs.
    std::function<random::RandomSource()> entityRandomFactory;

    [[nodiscard]] const SpawnEquipmentFinalizer& equipmentOrFallback() const;

    [[nodiscard]] random::RandomSource entityRandom() const;

    /// The biome's path, which is what every biome table here is keyed by.
    [[nodiscard]] std::string_view biomePath() const;
};

/// Minecraft 26.2 spawn finalization, including concrete pre and post layers around shared mob
/// state.
namespace mobSpawnFinalization {

/// What the common layers decided, before any of it is written anywhere.
struct CommonSpawnState {
    std::int32_t age = 0;
    std::int32_t cubeSize = 1;
    double followRangeBonus = 0.0;
    bool leftHanded = false;
    /// parity: `CaveSpider.finalizeSpawn` returns without calling `Mob`'s at all, so nothing below
    /// it ran and nothing below it may be written.
    bool ranMobFinalizer = false;
    wire::nbt::NbtCompound typeData;
    std::map<std::string, double> attributeBases;
};

/// Applies the common superclass finalizers in their exact order: `AgeableMob`'s five-percent baby
/// roll for every pack member after the first, then `Mob`'s triangular follow-range bonus and
/// left-handedness roll, then `AbstractCubeMob`'s size selection. Variants and equipment are their
/// own layers.
///
/// Answers the passengers the finalization created, which the caller spawns alongside it.
[[nodiscard]] std::vector<server::world::ChunkGeneratedEntity>
apply(entity::ServerEntity& entity, std::int32_t groupIndex, const MobSpawnContext& context, MobSpawnGroupData& group);

/// Writes finalization state into an off-thread generated entity descriptor.
[[nodiscard]] std::vector<server::world::ChunkGeneratedEntity>
writeGenerated(wire::nbt::NbtCompound& data, const wire::common::Identifier& type, std::int32_t groupIndex,
               const MobSpawnContext& context, MobSpawnGroupData& group, std::int32_t blockX, std::int32_t blockY,
               std::int32_t blockZ, double entityX, double entityY, double entityZ, float entityYaw);

/// One node of the tree, with whatever it creates around it. The recursive entry point every
/// concrete finalizer calls back into.
void writeGeneratedNode(MobSpawnForestBuilder& builder, MobSpawnNode& node, std::int32_t groupIndex,
                        const MobSpawnContext& context, MobSpawnGroupData& group, std::int32_t blockX,
                        std::int32_t blockY, std::int32_t blockZ, random::RandomSource& entityRandom);

[[nodiscard]] CommonSpawnState stateFor(const wire::common::Identifier& type, std::int32_t groupIndex,
                                        const MobSpawnContext& context, MobSpawnGroupData& group, std::int32_t blockX,
                                        std::int32_t blockY, std::int32_t blockZ, random::RandomSource& entityRandom);

/// The `AgeableMobGroupData` selected by each concrete natural-spawn class, or nothing for the ones
/// that disable the roll entirely.
[[nodiscard]] std::optional<float> babySpawnChance(std::string_view type);

} // namespace mobSpawnFinalization

/// Builds the complete generated forest while preserving which descriptor is the requested mob.
[[nodiscard]] MobSpawnForest writeGeneratedForest(wire::nbt::NbtCompound& data, const wire::common::Identifier& type,
                                                  std::int32_t groupIndex, const MobSpawnContext& context,
                                                  MobSpawnGroupData& group, std::int32_t blockX, std::int32_t blockY,
                                                  std::int32_t blockZ, double entityX, double entityY, double entityZ,
                                                  float entityYaw, MobSpawnEnvironment environment = {});

/// Passenger trees and pack effects created by concrete mob finalizers after their superclass.
namespace mobJockeyFinalization {

void afterCommon(MobSpawnForestBuilder& builder, MobSpawnNode& node, std::int32_t groupIndex,
                 const MobSpawnContext& context, MobSpawnGroupData& group);

} // namespace mobJockeyFinalization

/// Minecraft 26.2's post-super cat variant and sound selection.
namespace catSpawnFinalizer {

void afterCommon(const CatSpawnContext& context, random::RandomSource& random, wire::nbt::NbtCompound& data);

} // namespace catSpawnFinalizer

/// Minecraft 26.2's concrete piglin state that runs before `Mob.finalizeSpawn`.
namespace piglinSpawnFinalizer {

[[nodiscard]] bool finalizeBeforeCommon(wire::nbt::NbtCompound& data, const MobSpawnContext& context,
                                        random::RandomSource& entityRandom);

} // namespace piglinSpawnFinalizer

/// Passenger topology created by Minecraft 26.2 equine finalizers before their superclass.
namespace equineSpawnFinalizer {

void beforeCommon(MobSpawnForestBuilder& builder, MobSpawnNode& horse, const MobSpawnContext& context);

} // namespace equineSpawnFinalizer

/// Minecraft 26.2's recursive strider jockey selection before its ageable and mob superclass.
namespace striderSpawnFinalizer {

void beforeCommon(MobSpawnForestBuilder& builder, MobSpawnNode& strider, const MobSpawnContext& context);

} // namespace striderSpawnFinalizer

/// Minecraft 26.2 villager data selected before the shared ageable and mob finalizers.
namespace villagerSpawnFinalizer {

void beforeCommon(std::string_view type, std::string_view biome, MobSpawnReason reason,
                  random::RandomSource& entityRandom, wire::nbt::NbtCompound& data);

/// The professions a zombie villager draws from, in the registry's own order.
[[nodiscard]] const std::vector<std::string>& professions();

} // namespace villagerSpawnFinalizer

/// Minecraft 26.2's pillager, patrolling-monster and raider finalization stack.
namespace pillagerSpawnFinalizer {

void beforeCommon(MobSpawnGroupData& group, const MobSpawnContext& context, random::RandomSource& entityRandom,
                  wire::nbt::NbtCompound& data);

} // namespace pillagerSpawnFinalizer

/// Concrete variant, birth-state and memory layers around the shared mob finalizer.
namespace mobVariantSpawnFinalizer {

/// Concrete finalizers whose observable state and RNG happen before their superclass call. Answers
/// whether this type's own finalizer decided it is a baby.
[[nodiscard]] bool beforeCommon(std::string_view type, const MobSpawnContext& context, MobSpawnGroupData& group,
                                wire::nbt::NbtCompound& data, std::int32_t blockX, std::int32_t blockY,
                                std::int32_t blockZ, random::RandomSource& entityRandom);

/// Concrete finalizers that run after their superclass has consumed the common mob RNG.
void afterCommon(std::string_view type, MobSpawnGroupData& group, random::RandomSource& random,
                 wire::nbt::NbtCompound& data);

} // namespace mobVariantSpawnFinalizer

/// Minecraft 26.2's zombie-family layers after the shared mob finalizer.
namespace zombieSpawnFinalizer {

void afterCommon(MobSpawnForestBuilder& builder, MobSpawnNode& node, MobSpawnGroupData& group,
                 const MobSpawnContext& context, random::RandomSource& entityRandom);

} // namespace zombieSpawnFinalizer

} // namespace basalt::server::runtime::world
