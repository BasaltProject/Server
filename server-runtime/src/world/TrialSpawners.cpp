#include <basalt/server/runtime/world/TrialSpawners.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <limits>

namespace basalt::server::runtime::world::trialSpawners {
namespace {

using wire::common::Identifier;
using wire::common::Uuid;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::array<std::string_view, 6> kNames{
    "inactive", "waiting_for_players", "active", "waiting_for_reward_ejection", "ejecting_reward", "cooldown"};

constexpr std::string_view kDefaultConsumables = "minecraft:spawners/trial_chamber/consumables";
constexpr std::string_view kDefaultKey = "minecraft:spawners/trial_chamber/key";
constexpr std::string_view kDefaultOminousItems = "minecraft:spawners/trial_chamber/items_to_drop_when_ominous";

[[nodiscard]] std::vector<std::int32_t> intsOf(const Uuid& id) {
    return {static_cast<std::int32_t>(id.high() >> 32), static_cast<std::int32_t>(id.high()),
            static_cast<std::int32_t>(id.low() >> 32), static_cast<std::int32_t>(id.low())};
}

[[nodiscard]] std::optional<Uuid> uuidOf(const NbtTag& tag) {
    const auto* ints = tag.get<std::vector<std::int32_t>>();
    if (ints == nullptr || ints->size() != 4) {
        return std::nullopt;
    }
    const auto part = [](std::int32_t value) { return static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)); };
    return Uuid{(part((*ints)[0]) << 32) | part((*ints)[1]), (part((*ints)[2]) << 32) | part((*ints)[3])};
}

[[nodiscard]] std::vector<Uuid> readUuids(const NbtCompound& data, std::string_view key) {
    std::vector<Uuid> out;
    if (const NbtList* list = data.getList(key); list != nullptr) {
        for (const NbtTag& tag : *list) {
            if (const std::optional<Uuid> id = uuidOf(tag);
                id.has_value() && std::ranges::find(out, *id) == out.end()) {
                out.push_back(*id);
            }
        }
    }
    return out;
}

void writeUuids(NbtCompound& data, std::string_view key, const std::vector<Uuid>& ids) {
    if (ids.empty()) {
        static_cast<void>(data.remove(key));
        return;
    }
    std::vector<NbtTag> tags;
    for (const Uuid& id : ids) {
        tags.push_back(NbtTag::ofIntArray(intsOf(id)));
    }
    data.set(key, NbtTag::ofList(NbtList{NbtType::IntArray, std::move(tags)}));
}

[[nodiscard]] float floatOr(const NbtCompound& data, std::string_view key, float fallback) {
    const NbtTag* tag = data.find(key);
    const std::optional<double> value = tag == nullptr ? std::nullopt : baseSpawnerData::doubleValue(*tag);
    return value.has_value() ? static_cast<float>(std::max(*value, 0.0)) : fallback;
}

[[nodiscard]] std::int32_t intOr(const NbtCompound& data, std::string_view key, std::int32_t fallback) {
    const NbtTag* tag = data.find(key);
    const std::optional<std::int32_t> value = tag == nullptr ? std::nullopt : baseSpawnerData::intValue(*tag);
    return value.value_or(fallback);
}

[[nodiscard]] std::string qualified(std::string_view name) {
    return name.find(':') == std::string_view::npos ? "minecraft:" + std::string{name} : std::string{name};
}

[[nodiscard]] Config defaultConfig() {
    Config config;
    config.lootTablesToEject = {WeightedTable{std::string{kDefaultConsumables}, 1},
                                WeightedTable{std::string{kDefaultKey}, 1}};
    config.itemsToDropWhenOminous = std::string{kDefaultOminousItems};
    return config;
}

[[nodiscard]] Config configOf(const NbtTag& tag, const ConfigLookup& lookup) {
    if (const auto* inline_ = tag.get<NbtCompound>(); inline_ != nullptr) {
        return readConfig(*inline_);
    }
    if (const auto* key = tag.get<std::string>(); key != nullptr && lookup) {
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(*key);
        if (id.has_value()) {
            if (const NbtCompound* found = lookup(*id); found != nullptr) {
                return readConfig(*found);
            }
        }
    }
    return defaultConfig();
}

} // namespace

std::string_view nameOf(const State state) noexcept { return kNames[static_cast<std::size_t>(state)]; }

std::optional<State> stateByName(const std::string_view name) noexcept {
    const auto found = std::ranges::find(kNames, name);
    if (found == kNames.end()) {
        return std::nullopt;
    }
    return static_cast<State>(std::distance(kNames.begin(), found));
}

bool capableOfSpawning(const State state) noexcept {
    return state == State::WaitingForPlayers || state == State::Active;
}

std::int32_t Config::targetTotalMobs(const std::int32_t additionalPlayers) const {
    return static_cast<std::int32_t>(
        std::floor(static_cast<double>(totalMobs + (totalMobsAddedPerPlayer * static_cast<float>(additionalPlayers)))));
}

std::int32_t Config::targetSimultaneousMobs(const std::int32_t additionalPlayers) const {
    return static_cast<std::int32_t>(std::floor(static_cast<double>(
        simultaneousMobs + (simultaneousMobsAddedPerPlayer * static_cast<float>(additionalPlayers)))));
}

Config readConfig(const NbtCompound& data) {
    Config config = defaultConfig();
    config.spawnRange = std::clamp(intOr(data, "spawn_range", config.spawnRange), 1, 128);
    config.totalMobs = floatOr(data, "total_mobs", config.totalMobs);
    config.simultaneousMobs = floatOr(data, "simultaneous_mobs", config.simultaneousMobs);
    config.totalMobsAddedPerPlayer = floatOr(data, "total_mobs_added_per_player", config.totalMobsAddedPerPlayer);
    config.simultaneousMobsAddedPerPlayer =
        floatOr(data, "simultaneous_mobs_added_per_player", config.simultaneousMobsAddedPerPlayer);
    config.ticksBetweenSpawn = std::max(intOr(data, "ticks_between_spawn", config.ticksBetweenSpawn), 0);
    config.spawnPotentials.clear();
    if (const NbtList* list = data.getList("spawn_potentials"); list != nullptr) {
        for (const NbtTag& entry : *list) {
            const auto* object = entry.get<NbtCompound>();
            const NbtTag* spawn = object == nullptr ? nullptr : object->find("data");
            const NbtTag* weight = object == nullptr ? nullptr : object->find("weight");
            std::optional<SpawnData> read = spawn == nullptr ? std::nullopt : baseSpawnerData::readSpawnData(*spawn);
            if (!read.has_value()) {
                continue;
            }
            config.spawnPotentials.push_back(WeightedSpawnData{
                .data = std::move(*read),
                .weight = std::max(weight == nullptr ? 1 : baseSpawnerData::intValue(*weight).value_or(1), 1)});
        }
    }
    if (const NbtList* list = data.getList("loot_tables_to_eject"); list != nullptr) {
        config.lootTablesToEject.clear();
        for (const NbtTag& entry : *list) {
            const auto* object = entry.get<NbtCompound>();
            const NbtTag* table = object == nullptr ? nullptr : object->find("data");
            const auto* name = table == nullptr ? nullptr : table->get<std::string>();
            const NbtTag* weight = object == nullptr ? nullptr : object->find("weight");
            if (name != nullptr) {
                config.lootTablesToEject.push_back(
                    WeightedTable{qualified(*name),
                                  std::max(weight == nullptr ? 1 : baseSpawnerData::intValue(*weight).value_or(1), 1)});
            }
        }
    }
    if (const NbtTag* items = data.find("items_to_drop_when_ominous"); items != nullptr) {
        if (const auto* name = items->get<std::string>(); name != nullptr) {
            config.itemsToDropWhenOminous = qualified(*name);
        }
    }
    return config;
}

Full readFull(const NbtCompound& data, const ConfigLookup& lookup) {
    Full full{.normal = defaultConfig(), .ominous = defaultConfig()};
    if (const NbtTag* normal = data.find("normal_config"); normal != nullptr) {
        full.normal = configOf(*normal, lookup);
    }
    if (const NbtTag* ominous = data.find("ominous_config"); ominous != nullptr) {
        full.ominous = configOf(*ominous, lookup);
    }
    full.targetCooldownLength = std::max(intOr(data, "target_cooldown_length", full.targetCooldownLength), 0);
    full.requiredPlayerRange = std::clamp(intOr(data, "required_player_range", full.requiredPlayerRange), 1, 128);
    return full;
}

Data Data::read(const NbtCompound& data) {
    Data out;
    out.detectedPlayers = readUuids(data, "registered_players");
    out.currentMobs = readUuids(data, "current_mobs");
    out.cooldownEndsAt = data.getLong("cooldown_ends_at", 0);
    out.nextMobSpawnsAt = data.getLong("next_mob_spawns_at", 0);
    out.totalMobsSpawned = std::max(data.getInt("total_mobs_spawned", 0), 0);
    if (const NbtTag* spawn = data.find("spawn_data"); spawn != nullptr) {
        out.nextSpawnData = baseSpawnerData::readSpawnData(*spawn);
    }
    if (const NbtTag* loot = data.find("ejecting_loot_table"); loot != nullptr) {
        if (const auto* name = loot->get<std::string>(); name != nullptr) {
            out.ejectingLootTable = qualified(*name);
        }
    }
    return out;
}

void Data::write(NbtCompound& data) const {
    writeUuids(data, "registered_players", detectedPlayers);
    writeUuids(data, "current_mobs", currentMobs);
    data.putLong("cooldown_ends_at", cooldownEndsAt);
    data.putLong("next_mob_spawns_at", nextMobSpawnsAt);
    data.putInt("total_mobs_spawned", totalMobsSpawned);
    if (nextSpawnData.has_value()) {
        data.set("spawn_data", NbtTag::ofCompound(baseSpawnerData::writeSpawnData(*nextSpawnData)));
    } else {
        static_cast<void>(data.remove("spawn_data"));
    }
    if (ejectingLootTable.has_value()) {
        data.putString("ejecting_loot_table", *ejectingLootTable);
    } else {
        static_cast<void>(data.remove("ejecting_loot_table"));
    }
}

void Data::reset() {
    currentMobs.clear();
    nextSpawnData.reset();
    resetStatistics();
}

void Data::resetStatistics() {
    detectedPlayers.clear();
    totalMobsSpawned = 0;
    nextMobSpawnsAt = 0;
    cooldownEndsAt = 0;
}

const SpawnData& Data::nextOrPick(const Config& config, random::RandomSource& random) {
    if (!nextSpawnData.has_value()) {
        const SpawnData* picked =
            config.spawnPotentials.empty() ? nullptr : baseSpawnerData::pick(config.spawnPotentials, random);
        nextSpawnData = picked == nullptr ? SpawnData{} : *picked;
    }
    return *nextSpawnData;
}

const WeightedTable* pickWeighted(const std::vector<WeightedTable>& entries, random::RandomSource& random) {
    std::int64_t total = 0;
    for (const WeightedTable& entry : entries) {
        total += entry.weight;
    }
    if (total <= 0 || total > std::numeric_limits<std::int32_t>::max()) {
        return nullptr;
    }
    std::int32_t selection = random.nextInt(static_cast<std::int32_t>(total));
    for (const WeightedTable& entry : entries) {
        selection -= entry.weight;
        if (selection < 0) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace basalt::server::runtime::world::trialSpawners
