#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BaseSpawnerData.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `TrialSpawner`, `TrialSpawnerConfig`, `TrialSpawnerStateData` and `TrialSpawnerState`, as data.
namespace trialSpawners {

enum class State : std::uint8_t {
    Inactive,
    WaitingForPlayers,
    Active,
    WaitingForRewardEjection,
    EjectingReward,
    Cooldown
};

[[nodiscard]] std::string_view nameOf(State state) noexcept;

[[nodiscard]] std::optional<State> stateByName(std::string_view name) noexcept;

/// parity: `TrialSpawnerState`'s light level, particle style and whether it spawns.
[[nodiscard]] bool capableOfSpawning(State state) noexcept;

struct WeightedTable {
    std::string table;
    std::int32_t weight = 1;
};

struct Config {
    std::int32_t spawnRange = 4;
    float totalMobs = 6.0F;
    float simultaneousMobs = 2.0F;
    float totalMobsAddedPerPlayer = 2.0F;
    float simultaneousMobsAddedPerPlayer = 1.0F;
    std::int32_t ticksBetweenSpawn = 40;
    std::vector<WeightedSpawnData> spawnPotentials;
    std::vector<WeightedTable> lootTablesToEject;
    std::string itemsToDropWhenOminous;

    [[nodiscard]] std::int32_t targetTotalMobs(std::int32_t additionalPlayers) const;

    [[nodiscard]] std::int32_t targetSimultaneousMobs(std::int32_t additionalPlayers) const;
};

struct Full {
    Config normal;
    Config ominous;
    std::int32_t targetCooldownLength = 36000;
    std::int32_t requiredPlayerRange = 14;
};

/// An entry of the `trial_spawner` registry by id, null when there is none.
using ConfigLookup = std::function<const wire::nbt::NbtCompound*(const wire::common::Identifier&)>;

[[nodiscard]] Config readConfig(const wire::nbt::NbtCompound& data);

/// parity: `FullConfig.MAP_CODEC`; a config named by key is looked up, and a key nobody has reads as the default.
[[nodiscard]] Full readFull(const wire::nbt::NbtCompound& data, const ConfigLookup& lookup);

struct Data {
    std::vector<wire::common::Uuid> detectedPlayers;
    std::vector<wire::common::Uuid> currentMobs;
    std::int64_t cooldownEndsAt = 0;
    std::int64_t nextMobSpawnsAt = 0;
    std::int32_t totalMobsSpawned = 0;
    std::optional<SpawnData> nextSpawnData;
    std::optional<std::string> ejectingLootTable;

    friend bool operator==(const Data&, const Data&) = default;

    [[nodiscard]] static Data read(const wire::nbt::NbtCompound& data);

    void write(wire::nbt::NbtCompound& data) const;

    void reset();

    void resetStatistics();

    /// parity: `TrialSpawnerStateData.getOrCreateNextSpawnData`.
    [[nodiscard]] const SpawnData& nextOrPick(const Config& config, random::RandomSource& random);
};

[[nodiscard]] const WeightedTable* pickWeighted(const std::vector<WeightedTable>& entries,
                                                random::RandomSource& random);

} // namespace trialSpawners

} // namespace basalt::server::runtime::world
