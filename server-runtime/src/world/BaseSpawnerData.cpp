#include <basalt/server/runtime/world/BaseSpawnerData.hpp>

#include <algorithm>
#include <cmath>
#include <expected>
#include <limits>
#include <string>

namespace basalt::server::runtime::world::baseSpawnerData {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::int32_t kDefaultDelay = 20;
constexpr LightLimit kFullLightRange{};

/// parity: `Mth.floor(double)`, whose `(int)` cast saturates and turns NaN into zero.
[[nodiscard]] std::int32_t javaFloor(double value) {
    if (std::isnan(value)) {
        return 0;
    }
    const double floored = std::floor(value);
    if (floored >= static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
        return std::numeric_limits<std::int32_t>::max();
    }
    if (floored <= static_cast<double>(std::numeric_limits<std::int32_t>::min())) {
        return std::numeric_limits<std::int32_t>::min();
    }
    return static_cast<std::int32_t>(floored);
}

/// parity: `TagValueInput.getIntOr`, any number at all.
[[nodiscard]] std::int32_t intOr(const NbtCompound& data, std::string_view key, std::int32_t fallback) {
    const NbtTag* tag = data.find(key);
    const std::optional<std::int32_t> value = tag == nullptr ? std::nullopt : intValue(*tag);
    return value.value_or(fallback);
}

/// parity: `TagValueInput.getShortOr`, the low sixteen bits of the number's int value.
[[nodiscard]] std::int32_t shortOr(const NbtCompound& data, std::string_view key, std::int32_t fallback) {
    const NbtTag* tag = data.find(key);
    const std::optional<std::int32_t> value = tag == nullptr ? std::nullopt : intValue(*tag);
    return value.has_value() ? static_cast<std::int16_t>(static_cast<std::uint16_t>(*value & 0xFFFF)) : fallback;
}

/// parity: `ExtraCodecs.intervalCodec`: one number, a pair, or `min_inclusive`/`max_inclusive`.
[[nodiscard]] std::optional<LightLimit> readInterval(const NbtTag& tag) {
    std::optional<std::int32_t> low;
    std::optional<std::int32_t> high;
    if (const std::optional<std::int32_t> single = intValue(tag); single.has_value()) {
        low = single;
        high = single;
    } else if (const auto* list = tag.get<NbtList>(); list != nullptr && list->size() == 2) {
        low = intValue((*list)[0]);
        high = intValue((*list)[1]);
    } else if (const auto* ints = tag.get<std::vector<std::int32_t>>(); ints != nullptr && ints->size() == 2) {
        low = (*ints)[0];
        high = (*ints)[1];
    } else if (const auto* object = tag.get<NbtCompound>(); object != nullptr) {
        const NbtTag* min = object->find("min_inclusive");
        const NbtTag* max = object->find("max_inclusive");
        low = min == nullptr ? std::nullopt : intValue(*min);
        high = max == nullptr ? std::nullopt : intValue(*max);
    }
    // parity: `InclusiveRange.create` refuses a window whose ends are the wrong way round.
    if (!low.has_value() || !high.has_value() || *low > *high) {
        return std::nullopt;
    }
    return LightLimit{.min = *low, .max = *high};
}

[[nodiscard]] NbtTag writeInterval(const LightLimit& limit) {
    if (limit.min == limit.max) {
        return NbtTag::ofInt(limit.min);
    }
    NbtCompound object;
    object.putInt("min_inclusive", limit.min);
    object.putInt("max_inclusive", limit.max);
    return NbtTag::ofCompound(std::move(object));
}

/// parity: `CustomSpawnRules.lightLimit`: lenient, so an unreadable window is the full one, and then
/// validated, so a window reaching outside zero to fifteen fails the whole record.
[[nodiscard]] std::optional<LightLimit> readLightLimit(const NbtCompound& rules, std::string_view key) {
    const NbtTag* tag = rules.find(key);
    const LightLimit limit = tag == nullptr ? kFullLightRange : readInterval(*tag).value_or(kFullLightRange);
    if (limit.min < kFullLightRange.min || limit.max > kFullLightRange.max) {
        return std::nullopt;
    }
    return limit;
}

[[nodiscard]] std::optional<CustomSpawnRules> readRules(const NbtTag& tag) {
    const auto* rules = tag.get<NbtCompound>();
    if (rules == nullptr) {
        return std::nullopt;
    }
    const std::optional<LightLimit> block = readLightLimit(*rules, "block_light_limit");
    const std::optional<LightLimit> sky = readLightLimit(*rules, "sky_light_limit");
    if (!block.has_value() || !sky.has_value()) {
        return std::nullopt;
    }
    return CustomSpawnRules{.blockLight = *block, .skyLight = *sky};
}

[[nodiscard]] NbtCompound writeRules(const CustomSpawnRules& rules) {
    NbtCompound tag;
    // parity: an optional field equal to its default is left out.
    if (rules.blockLight != kFullLightRange) {
        tag.set("block_light_limit", writeInterval(rules.blockLight));
    }
    if (rules.skyLight != kFullLightRange) {
        tag.set("sky_light_limit", writeInterval(rules.skyLight));
    }
    return tag;
}

[[nodiscard]] std::optional<float> floatValue(const NbtTag& tag) {
    const std::optional<double> value = doubleValue(tag);
    return value.has_value() ? std::optional{static_cast<float>(*value)} : std::nullopt;
}

/// parity: `EquipmentTable.DROP_CHANCES_CODEC`, one chance for every slot or a map of them.
[[nodiscard]] std::optional<std::vector<std::pair<std::string, float>>> readDropChances(const NbtTag& tag) {
    std::vector<std::pair<std::string, float>> chances;
    if (const std::optional<float> all = floatValue(tag); all.has_value()) {
        for (const std::string_view slot : kSlots) {
            chances.emplace_back(std::string{slot}, *all);
        }
        return chances;
    }
    const auto* map = tag.get<NbtCompound>();
    if (map == nullptr) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < map->size(); ++index) {
        const std::string& slot = map->names()[index];
        const std::optional<float> chance = floatValue(map->values()[index]);
        if (std::ranges::find(kSlots, slot) == kSlots.end() || !chance.has_value()) {
            return std::nullopt;
        }
        chances.emplace_back(slot, *chance);
    }
    return chances;
}

[[nodiscard]] std::optional<EquipmentTable> readEquipment(const NbtTag& tag) {
    const auto* table = tag.get<NbtCompound>();
    const NbtTag* key = table == nullptr ? nullptr : table->find("loot_table");
    const auto* name = key == nullptr ? nullptr : key->get<std::string>();
    if (name == nullptr) {
        return std::nullopt;
    }
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(*name);
    if (!id.has_value()) {
        return std::nullopt;
    }
    EquipmentTable equipment{.lootTable = *id};
    if (const NbtTag* chances = table->find("slot_drop_chances"); chances != nullptr) {
        std::optional<std::vector<std::pair<std::string, float>>> read = readDropChances(*chances);
        if (!read.has_value()) {
            return std::nullopt;
        }
        equipment.slotDropChances = std::move(*read);
    }
    return equipment;
}

[[nodiscard]] NbtCompound writeEquipment(const EquipmentTable& equipment) {
    NbtCompound tag;
    tag.putString("loot_table", equipment.lootTable.toString());
    const std::vector<std::pair<std::string, float>>& chances = equipment.slotDropChances;
    if (chances.empty()) {
        return tag;
    }
    // parity: the same chance in every one of the eight slots is written as that one number.
    const bool allSlots = std::ranges::all_of(
        kSlots, [&equipment](std::string_view slot) { return equipment.dropChanceOf(slot).has_value(); });
    const bool allSame = std::ranges::all_of(
        chances, [&chances](const std::pair<std::string, float>& entry) { return entry.second == chances[0].second; });
    if (allSlots && allSame) {
        tag.putFloat("slot_drop_chances", chances[0].second);
        return tag;
    }
    NbtCompound map;
    for (const std::string_view slot : kSlots) {
        if (const std::optional<float> chance = equipment.dropChanceOf(slot); chance.has_value()) {
            map.putFloat(slot, *chance);
        }
    }
    tag.set("slot_drop_chances", NbtTag::ofCompound(std::move(map)));
    return tag;
}

[[nodiscard]] std::optional<WeightedSpawnData> readWeighted(const NbtTag& tag) {
    const auto* entry = tag.get<NbtCompound>();
    const NbtTag* data = entry == nullptr ? nullptr : entry->find("data");
    const NbtTag* weight = entry == nullptr ? nullptr : entry->find("weight");
    if (data == nullptr || weight == nullptr) {
        return std::nullopt;
    }
    std::optional<SpawnData> spawn = readSpawnData(*data);
    const std::optional<std::int32_t> amount = intValue(*weight);
    // parity: `ExtraCodecs.NON_NEGATIVE_INT`.
    if (!spawn.has_value() || !amount.has_value() || *amount < 0) {
        return std::nullopt;
    }
    return WeightedSpawnData{.data = std::move(*spawn), .weight = *amount};
}

} // namespace

std::optional<std::int32_t> intValue(const NbtTag& tag) {
    switch (tag.type()) {
    case NbtType::Byte:
        return *tag.get<std::int8_t>();
    case NbtType::Short:
        return *tag.get<std::int16_t>();
    case NbtType::Int:
        return *tag.get<std::int32_t>();
    case NbtType::Long:
        return static_cast<std::int32_t>(static_cast<std::uint32_t>(*tag.get<std::int64_t>()));
    case NbtType::Float:
        return javaFloor(static_cast<double>(*tag.get<float>()));
    case NbtType::Double:
        return javaFloor(*tag.get<double>());
    default:
        return std::nullopt;
    }
}

std::optional<double> doubleValue(const NbtTag& tag) {
    switch (tag.type()) {
    case NbtType::Byte:
        return *tag.get<std::int8_t>();
    case NbtType::Short:
        return *tag.get<std::int16_t>();
    case NbtType::Int:
        return *tag.get<std::int32_t>();
    case NbtType::Long:
        return static_cast<double>(*tag.get<std::int64_t>());
    case NbtType::Float:
        return static_cast<double>(*tag.get<float>());
    case NbtType::Double:
        return *tag.get<double>();
    default:
        return std::nullopt;
    }
}

std::optional<SpawnData> readSpawnData(const NbtTag& tag) {
    const auto* root = tag.get<NbtCompound>();
    const NbtCompound* entity = root == nullptr ? nullptr : root->getCompound("entity");
    if (entity == nullptr) {
        return std::nullopt;
    }
    SpawnData data{.entity = *entity};
    // parity: the `SpawnData` constructor keeps an `id` only in its normalized form.
    const NbtTag* idTag = data.entity.find("id");
    const auto* idText = idTag == nullptr ? nullptr : idTag->get<std::string>();
    std::optional<Identifier> id;
    if (idText != nullptr) {
        if (const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(*idText);
            parsed.has_value()) {
            id = *parsed;
        }
    }
    if (id.has_value()) {
        data.entity.putString("id", id->toString());
    } else {
        static_cast<void>(data.entity.remove("id"));
    }
    if (const NbtTag* rules = root->find("custom_spawn_rules"); rules != nullptr) {
        data.customSpawnRules = readRules(*rules);
        if (!data.customSpawnRules.has_value()) {
            return std::nullopt;
        }
    }
    if (const NbtTag* equipment = root->find("equipment"); equipment != nullptr) {
        data.equipment = readEquipment(*equipment);
        if (!data.equipment.has_value()) {
            return std::nullopt;
        }
    }
    return data;
}

NbtCompound writeSpawnData(const SpawnData& data) {
    NbtCompound tag;
    tag.set("entity", NbtTag::ofCompound(data.entity));
    if (data.customSpawnRules.has_value()) {
        tag.set("custom_spawn_rules", NbtTag::ofCompound(writeRules(*data.customSpawnRules)));
    }
    if (data.equipment.has_value()) {
        tag.set("equipment", NbtTag::ofCompound(writeEquipment(*data.equipment)));
    }
    return tag;
}

BaseSpawnerState load(const NbtCompound& data) {
    BaseSpawnerState state;
    state.spawnDelay = shortOr(data, "Delay", kDefaultDelay);
    if (const NbtTag* next = data.find("SpawnData"); next != nullptr) {
        state.nextSpawnData = readSpawnData(*next);
    }
    // parity: a list keeps the entries that decode, the codec's partial result; anything else
    // falls back to the next spawn alone.
    const NbtTag* potentials = data.find("SpawnPotentials");
    if (const auto* list = potentials == nullptr ? nullptr : potentials->get<NbtList>(); list != nullptr) {
        for (const NbtTag& element : *list) {
            if (std::optional<WeightedSpawnData> entry = readWeighted(element); entry.has_value()) {
                state.spawnPotentials.push_back(std::move(*entry));
            }
        }
    } else {
        state.spawnPotentials.push_back(
            WeightedSpawnData{.data = state.nextSpawnData.value_or(SpawnData{}), .weight = 1});
    }
    state.minSpawnDelay = intOr(data, "MinSpawnDelay", state.minSpawnDelay);
    state.maxSpawnDelay = intOr(data, "MaxSpawnDelay", state.maxSpawnDelay);
    state.spawnCount = intOr(data, "SpawnCount", state.spawnCount);
    state.maxNearbyEntities = intOr(data, "MaxNearbyEntities", state.maxNearbyEntities);
    state.requiredPlayerRange = intOr(data, "RequiredPlayerRange", state.requiredPlayerRange);
    state.spawnRange = intOr(data, "SpawnRange", state.spawnRange);
    return state;
}

void save(const BaseSpawnerState& state, NbtCompound& data) {
    const auto asShort = [](std::int32_t value) { return static_cast<std::int16_t>(value); };
    data.putShort("Delay", asShort(state.spawnDelay));
    data.putShort("MinSpawnDelay", asShort(state.minSpawnDelay));
    data.putShort("MaxSpawnDelay", asShort(state.maxSpawnDelay));
    data.putShort("SpawnCount", asShort(state.spawnCount));
    data.putShort("MaxNearbyEntities", asShort(state.maxNearbyEntities));
    data.putShort("RequiredPlayerRange", asShort(state.requiredPlayerRange));
    data.putShort("SpawnRange", asShort(state.spawnRange));
    if (state.nextSpawnData.has_value()) {
        data.set("SpawnData", NbtTag::ofCompound(writeSpawnData(*state.nextSpawnData)));
    } else {
        static_cast<void>(data.remove("SpawnData"));
    }
    NbtList potentials{state.spawnPotentials.empty() ? NbtType::End : NbtType::Compound};
    for (const WeightedSpawnData& entry : state.spawnPotentials) {
        NbtCompound weighted;
        weighted.set("data", NbtTag::ofCompound(writeSpawnData(entry.data)));
        weighted.putInt("weight", entry.weight);
        static_cast<void>(potentials.push(NbtTag::ofCompound(std::move(weighted))));
    }
    data.set("SpawnPotentials", NbtTag::ofList(std::move(potentials)));
}

const SpawnData* pick(const std::vector<WeightedSpawnData>& entries, random::RandomSource& random) {
    std::int64_t total = 0;
    for (const WeightedSpawnData& entry : entries) {
        total += entry.weight;
    }
    if (total <= 0 || total > std::numeric_limits<std::int32_t>::max()) {
        return nullptr;
    }
    std::int32_t selection = random.nextInt(static_cast<std::int32_t>(total));
    for (const WeightedSpawnData& entry : entries) {
        selection -= entry.weight;
        if (selection < 0) {
            return &entry.data;
        }
    }
    return nullptr;
}

} // namespace basalt::server::runtime::world::baseSpawnerData

namespace basalt::server::runtime::world {

std::optional<float> EquipmentTable::dropChanceOf(std::string_view slot) const {
    for (const auto& [name, chance] : slotDropChances) {
        if (name == slot) {
            return chance;
        }
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::world
