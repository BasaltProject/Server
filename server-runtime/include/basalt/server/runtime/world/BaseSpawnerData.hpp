#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

struct LightLimit {
    std::int32_t min = 0;
    std::int32_t max = 15;

    [[nodiscard]] bool contains(std::int32_t level) const noexcept { return level >= min && level <= max; }

    friend bool operator==(const LightLimit&, const LightLimit&) = default;
};

struct CustomSpawnRules {
    LightLimit blockLight;
    LightLimit skyLight;

    friend bool operator==(const CustomSpawnRules&, const CustomSpawnRules&) = default;
};

struct EquipmentTable {
    wire::common::Identifier lootTable{"minecraft", "empty"};
    std::vector<std::pair<std::string, float>> slotDropChances;

    [[nodiscard]] std::optional<float> dropChanceOf(std::string_view slot) const;

    friend bool operator==(const EquipmentTable&, const EquipmentTable&) = default;
};

struct SpawnData {
    wire::nbt::NbtCompound entity;
    std::optional<CustomSpawnRules> customSpawnRules;
    std::optional<EquipmentTable> equipment;

    friend bool operator==(const SpawnData&, const SpawnData&) = default;
};

struct WeightedSpawnData {
    SpawnData data;
    std::int32_t weight = 1;

    friend bool operator==(const WeightedSpawnData&, const WeightedSpawnData&) = default;
};

struct BaseSpawnerState {
    std::int32_t spawnDelay = 20;
    std::optional<SpawnData> nextSpawnData;
    std::vector<WeightedSpawnData> spawnPotentials;
    std::int32_t minSpawnDelay = 200;
    std::int32_t maxSpawnDelay = 800;
    std::int32_t spawnCount = 4;
    std::int32_t maxNearbyEntities = 6;
    std::int32_t requiredPlayerRange = 16;
    std::int32_t spawnRange = 4;
};

/// parity: `BaseSpawner.load` and `save`, with vanilla's names and defaults: `Delay`.
namespace baseSpawnerData {

inline constexpr std::array<std::string_view, 8> kSlots{"mainhand", "offhand", "feet", "legs",
                                                        "chest",    "head",    "body", "saddle"};

[[nodiscard]] BaseSpawnerState load(const wire::nbt::NbtCompound& data);

void save(const BaseSpawnerState& state, wire::nbt::NbtCompound& data);

/// parity: `SpawnData.CODEC`.
[[nodiscard]] std::optional<SpawnData> readSpawnData(const wire::nbt::NbtTag& tag);

[[nodiscard]] wire::nbt::NbtCompound writeSpawnData(const SpawnData& data);

[[nodiscard]] const SpawnData* pick(const std::vector<WeightedSpawnData>& entries, random::RandomSource& random);

[[nodiscard]] std::optional<std::int32_t> intValue(const wire::nbt::NbtTag& tag);

[[nodiscard]] std::optional<double> doubleValue(const wire::nbt::NbtTag& tag);

} // namespace baseSpawnerData

} // namespace basalt::server::runtime::world
