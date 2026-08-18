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

namespace basalt::server::runtime::config {
struct MobSettings;
}

namespace basalt::server::runtime::world {

struct CatSpawnContext {
    float moonBrightness = 0.0F;
    bool inBlackCatStructure = false;
};

/// parity: what one pack agrees on, so a flock shares a colour and a horde's later zombies are babies if the first was.
struct MobSpawnGroupData {
    std::optional<std::int32_t> variant;
    std::optional<std::int32_t> secondaryVariant;
    std::optional<std::string> spiderEffect;
    std::optional<bool> zombieBaby;
    bool zombieCanSpawnJockey = true;
    bool patrolLeader = false;
    CatSpawnContext catSpawnContext;
};

/// A struct rather than many arguments repeated down a recursion, where a wrong `reason` or swapped random hides.
struct MobSpawnContext {
    const config::MobSettings* mobs = nullptr;
    random::RandomSource* random = nullptr;
    /// parity: `specialMultiplier`.
    float specialMultiplier = 0.0F;
    std::string difficulty = "easy";
    const SpawnEquipmentFinalizer* equipment = nullptr;
    std::optional<wire::common::Identifier> biome;
    std::int64_t gameTime = 0;
    MobSpawnReason reason = MobSpawnReason::Natural;
    /// Vanilla seeds a fresh stream per mob it constructs.
    std::function<random::RandomSource()> entityRandomFactory;
    float moonBrightness = 0.0F;
    std::function<bool(const wire::common::Identifier& structure, std::int32_t x, std::int32_t y, std::int32_t z)>
        structureAt;

    [[nodiscard]] const SpawnEquipmentFinalizer& equipmentOrFallback() const;

    [[nodiscard]] random::RandomSource entityRandom() const;

    [[nodiscard]] std::string_view biomePath() const;
};

namespace mobSpawnFinalization {

struct CommonSpawnState {
    std::int32_t age = 0;
    std::int32_t cubeSize = 1;
    double followRangeBonus = 0.0;
    bool leftHanded = false;
    /// parity: `CaveSpider.finalizeSpawn`.
    bool ranMobFinalizer = false;
    wire::nbt::NbtCompound typeData;
    std::map<std::string, double> attributeBases;
};

/// parity: `AgeableMob`.
[[nodiscard]] std::vector<server::world::ChunkGeneratedEntity>
apply(entity::ServerEntity& entity, std::int32_t groupIndex, const MobSpawnContext& context, MobSpawnGroupData& group);

[[nodiscard]] std::vector<server::world::ChunkGeneratedEntity>
writeGenerated(wire::nbt::NbtCompound& data, const wire::common::Identifier& type, std::int32_t groupIndex,
               const MobSpawnContext& context, MobSpawnGroupData& group, std::int32_t blockX, std::int32_t blockY,
               std::int32_t blockZ, double entityX, double entityY, double entityZ, float entityYaw);

void writeGeneratedNode(MobSpawnForestBuilder& builder, MobSpawnNode& node, std::int32_t groupIndex,
                        const MobSpawnContext& context, MobSpawnGroupData& group, std::int32_t blockX,
                        std::int32_t blockY, std::int32_t blockZ, random::RandomSource& entityRandom);

[[nodiscard]] CommonSpawnState stateFor(const wire::common::Identifier& type, std::int32_t groupIndex,
                                        const MobSpawnContext& context, MobSpawnGroupData& group, std::int32_t blockX,
                                        std::int32_t blockY, std::int32_t blockZ, random::RandomSource& entityRandom);

/// The `AgeableMobGroupData` of each natural-spawn class, or nothing where the roll is disabled.
[[nodiscard]] std::optional<float> babySpawnChance(std::string_view type);

} // namespace mobSpawnFinalization

[[nodiscard]] MobSpawnForest writeGeneratedForest(wire::nbt::NbtCompound& data, const wire::common::Identifier& type,
                                                  std::int32_t groupIndex, const MobSpawnContext& context,
                                                  MobSpawnGroupData& group, std::int32_t blockX, std::int32_t blockY,
                                                  std::int32_t blockZ, double entityX, double entityY, double entityZ,
                                                  float entityYaw, MobSpawnEnvironment environment = {});

namespace mobJockeyFinalization {

void afterCommon(MobSpawnForestBuilder& builder, MobSpawnNode& node, std::int32_t groupIndex,
                 const MobSpawnContext& context, MobSpawnGroupData& group);

} // namespace mobJockeyFinalization

namespace catSpawnFinalizer {

void afterCommon(const CatSpawnContext& cat, const MobSpawnContext& context, std::int32_t blockX, std::int32_t blockY,
                 std::int32_t blockZ, random::RandomSource& random, wire::nbt::NbtCompound& data);

} // namespace catSpawnFinalizer

namespace piglinSpawnFinalizer {

[[nodiscard]] bool finalizeBeforeCommon(wire::nbt::NbtCompound& data, const MobSpawnContext& context,
                                        random::RandomSource& entityRandom);

} // namespace piglinSpawnFinalizer

namespace equineSpawnFinalizer {

void beforeCommon(MobSpawnForestBuilder& builder, MobSpawnNode& horse, const MobSpawnContext& context);

} // namespace equineSpawnFinalizer

namespace striderSpawnFinalizer {

void beforeCommon(MobSpawnForestBuilder& builder, MobSpawnNode& strider, const MobSpawnContext& context);

} // namespace striderSpawnFinalizer

namespace villagerSpawnFinalizer {

void beforeCommon(std::string_view type, std::string_view biome, MobSpawnReason reason,
                  random::RandomSource& entityRandom, wire::nbt::NbtCompound& data);

[[nodiscard]] const std::vector<std::string>& professions();

} // namespace villagerSpawnFinalizer

namespace pillagerSpawnFinalizer {

void beforeCommon(MobSpawnGroupData& group, const MobSpawnContext& context, random::RandomSource& entityRandom,
                  wire::nbt::NbtCompound& data);

} // namespace pillagerSpawnFinalizer

namespace mobVariantSpawnFinalizer {

/// Runs before the superclass call; answers whether the type's own finalizer decided it is a baby.
[[nodiscard]] bool beforeCommon(std::string_view type, const MobSpawnContext& context, MobSpawnGroupData& group,
                                wire::nbt::NbtCompound& data, std::int32_t blockX, std::int32_t blockY,
                                std::int32_t blockZ, random::RandomSource& entityRandom);

/// Runs after the superclass has consumed the common mob RNG.
void afterCommon(std::string_view type, MobSpawnGroupData& group, const MobSpawnContext& context, std::int32_t blockX,
                 std::int32_t blockY, std::int32_t blockZ, random::RandomSource& random, wire::nbt::NbtCompound& data);

} // namespace mobVariantSpawnFinalizer

namespace zombieSpawnFinalizer {

void afterCommon(MobSpawnForestBuilder& builder, MobSpawnNode& node, MobSpawnGroupData& group,
                 const MobSpawnContext& context, random::RandomSource& entityRandom);

} // namespace zombieSpawnFinalizer

} // namespace basalt::server::runtime::world
