#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/BaseSpawnerData.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::server::runtime::world;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;

namespace {

[[nodiscard]] NbtCompound entityOf(std::string id) {
    NbtCompound entity;
    entity.putString("id", std::move(id));
    return entity;
}

[[nodiscard]] NbtCompound spawnDataOf(std::string id) {
    NbtCompound data;
    data.set("entity", NbtTag::ofCompound(entityOf(std::move(id))));
    return data;
}

} // namespace

TEST_CASE("an empty spawner reads vanilla's defaults and one empty potential") {
    const BaseSpawnerState state = baseSpawnerData::load(NbtCompound{});
    CHECK(state.spawnDelay == 20);
    CHECK(state.minSpawnDelay == 200);
    CHECK(state.maxSpawnDelay == 800);
    CHECK(state.spawnCount == 4);
    CHECK(state.maxNearbyEntities == 6);
    CHECK(state.requiredPlayerRange == 16);
    CHECK(state.spawnRange == 4);
    CHECK_FALSE(state.nextSpawnData.has_value());
    REQUIRE(state.spawnPotentials.size() == 1);
    CHECK(state.spawnPotentials[0].weight == 1);
    CHECK(state.spawnPotentials[0].data.entity.empty());
}

TEST_CASE("a generated spawner keeps its empty potentials and normalizes the id") {
    NbtCompound data;
    data.set("SpawnData", NbtTag::ofCompound(spawnDataOf("blaze")));
    data.set("SpawnPotentials", NbtTag::ofList(NbtList{NbtType::End}));
    data.putInt("MinSpawnDelay", 300);
    const BaseSpawnerState state = baseSpawnerData::load(data);
    REQUIRE(state.nextSpawnData.has_value());
    CHECK(state.nextSpawnData->entity.getString("id") == "minecraft:blaze");
    CHECK(state.spawnPotentials.empty());
    CHECK(state.minSpawnDelay == 300);
}

TEST_CASE("every field survives a save and a load") {
    BaseSpawnerState state;
    state.spawnDelay = 57;
    state.maxNearbyEntities = 9;
    SpawnData zombie{.entity = entityOf("minecraft:zombie")};
    zombie.customSpawnRules = CustomSpawnRules{.blockLight = {.min = 0, .max = 7}, .skyLight = {.min = 3, .max = 3}};
    zombie.equipment = EquipmentTable{.lootTable = Identifier::vanilla("equipment/trial_chamber")};
    for (const std::string_view slot : baseSpawnerData::kSlots) {
        zombie.equipment->slotDropChances.emplace_back(std::string{slot}, 0.5F);
    }
    SpawnData skeleton{.entity = entityOf("minecraft:skeleton")};
    skeleton.equipment = EquipmentTable{.lootTable = Identifier::vanilla("equipment/trial_chamber"),
                                        .slotDropChances = {{"head", 1.0F}}};
    state.nextSpawnData = zombie;
    state.spawnPotentials = {{.data = zombie, .weight = 3}, {.data = skeleton, .weight = 1}};

    NbtCompound saved;
    baseSpawnerData::save(state, saved);
    CHECK(saved.find("Delay")->type() == NbtType::Short);
    // parity: the same chance in all eight slots is written as one number.
    const NbtCompound* equipment = saved.getCompound("SpawnData")->getCompound("equipment");
    REQUIRE(equipment != nullptr);
    CHECK(equipment->find("slot_drop_chances")->type() == NbtType::Float);

    const BaseSpawnerState loaded = baseSpawnerData::load(saved);
    CHECK(loaded.spawnDelay == 57);
    CHECK(loaded.maxNearbyEntities == 9);
    CHECK(loaded.nextSpawnData == state.nextSpawnData);
    CHECK(loaded.spawnPotentials == state.spawnPotentials);
}

TEST_CASE("a light window outside zero to fifteen fails the whole spawn data") {
    NbtCompound rules;
    rules.putInt("block_light_limit", 16);
    NbtCompound data = spawnDataOf("minecraft:zombie");
    data.set("custom_spawn_rules", NbtTag::ofCompound(std::move(rules)));
    CHECK_FALSE(baseSpawnerData::readSpawnData(NbtTag::ofCompound(std::move(data))).has_value());
}

TEST_CASE("a weightless list is picked from without a draw") {
    const std::vector<WeightedSpawnData> none{{.data = SpawnData{}, .weight = 0}};
    auto random = basalt::server::random::RandomSource::legacy(1);
    auto untouched = basalt::server::random::RandomSource::legacy(1);
    CHECK(baseSpawnerData::pick(none, random) == nullptr);
    CHECK(random.nextInt() == untouched.nextInt());
}
