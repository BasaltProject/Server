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

/// A window of light levels, both ends included. parity: `InclusiveRange<Integer>`.
struct LightLimit {
    std::int32_t min = 0;
    std::int32_t max = 15;

    [[nodiscard]] bool contains(std::int32_t level) const noexcept { return level >= min && level <= max; }

    friend bool operator==(const LightLimit&, const LightLimit&) = default;
};

/// parity: `SpawnData.CustomSpawnRules`, which replaces the type's own spawn rules with two light
/// windows.
struct CustomSpawnRules {
    LightLimit blockLight;
    LightLimit skyLight;

    friend bool operator==(const CustomSpawnRules&, const CustomSpawnRules&) = default;
};

/// parity: `EquipmentTable`: a loot table rolled onto the mob, and the drop chance of each slot it
/// may fill, by the slot's own name.
struct EquipmentTable {
    wire::common::Identifier lootTable{"minecraft", "empty"};
    std::vector<std::pair<std::string, float>> slotDropChances;

    [[nodiscard]] std::optional<float> dropChanceOf(std::string_view slot) const;

    friend bool operator==(const EquipmentTable&, const EquipmentTable&) = default;
};

/// parity: `SpawnData`, one thing a spawner can make.
struct SpawnData {
    wire::nbt::NbtCompound entity;
    std::optional<CustomSpawnRules> customSpawnRules;
    std::optional<EquipmentTable> equipment;

    friend bool operator==(const SpawnData&, const SpawnData&) = default;
};

/// One entry of `SpawnPotentials`. parity: `Weighted<SpawnData>`.
struct WeightedSpawnData {
    SpawnData data;
    std::int32_t weight = 1;

    friend bool operator==(const WeightedSpawnData&, const WeightedSpawnData&) = default;
};

/// Everything `BaseSpawner` keeps between ticks, with its defaults.
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

/// The saved form of a monster spawner, which is the same for the block and the minecart.
///
/// parity: `BaseSpawner.load` and `save`, with vanilla's names and defaults: `Delay`,
/// `MinSpawnDelay`, `MaxSpawnDelay`, `SpawnCount`, `MaxNearbyEntities`, `RequiredPlayerRange`,
/// `SpawnRange` (all shorts on save), `SpawnData` and `SpawnPotentials`.
namespace baseSpawnerData {

/// The eight equipment slots in `EquipmentSlot` order.
inline constexpr std::array<std::string_view, 8> kSlots{"mainhand", "offhand", "feet", "legs",
                                                        "chest",    "head",    "body", "saddle"};

/// Reads the fields the way `BaseSpawner.load` does: a missing or malformed `SpawnPotentials` falls
/// back to a list of the next spawn alone, or of an empty one.
[[nodiscard]] BaseSpawnerState load(const wire::nbt::NbtCompound& data);

/// Writes every field back into `data`, leaving whatever else it holds alone.
void save(const BaseSpawnerState& state, wire::nbt::NbtCompound& data);

/// parity: `SpawnData.CODEC`. Nothing when the tag is not one; the entity's `id` is normalized the
/// way the record's constructor does.
[[nodiscard]] std::optional<SpawnData> readSpawnData(const wire::nbt::NbtTag& tag);

[[nodiscard]] wire::nbt::NbtCompound writeSpawnData(const SpawnData& data);

/// parity: `WeightedList.getRandom`: no draw at all when the weights add up to nothing.
[[nodiscard]] const SpawnData* pick(const std::vector<WeightedSpawnData>& entries, random::RandomSource& random);

/// parity: `NumericTag.intValue`, integers wrapped and decimals floored. Nothing for a non-number.
[[nodiscard]] std::optional<std::int32_t> intValue(const wire::nbt::NbtTag& tag);

/// parity: `NumericTag.doubleValue`.
[[nodiscard]] std::optional<double> doubleValue(const wire::nbt::NbtTag& tag);

} // namespace baseSpawnerData

} // namespace basalt::server::runtime::world
