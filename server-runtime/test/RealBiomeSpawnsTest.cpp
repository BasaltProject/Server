#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>

using basalt::server::runtime::world::MobSpawnCost;
using basalt::server::runtime::world::RealBiomeSpawns;
using basalt::server::runtime::world::SpawnerEntry;
using basalt::wire::common::Identifier;

namespace {

/// A biome file shaped like the jar's own, with the parts worldgen owns left out.
constexpr std::string_view kPlains = R"({
  "temperature": 0.8,
  "downfall": 0.4,
  "creature_spawn_probability": 0.07,
  "spawners": {
    "monster": [
      {"type": "minecraft:zombie", "weight": 95, "minCount": 4, "maxCount": 4},
      {"type": "minecraft:skeleton", "weight": 100, "minCount": 4, "maxCount": 4}
    ],
    "creature": [
      {"type": "minecraft:sheep", "weight": 12, "minCount": 4, "maxCount": 4}
    ],
    "ambient": []
  },
  "spawn_costs": {
    "minecraft:magma_cube": {"energy_budget": 0.7, "charge": 0.15}
  }
})";

} // namespace

TEST_CASE("a biome's spawn lists are read by category") {
    const RealBiomeSpawns::Settings settings = RealBiomeSpawns::parse(kPlains);

    REQUIRE(settings.spawners.contains("monster"));
    REQUIRE(settings.spawners.at("monster").size() == 2);
    CHECK(settings.spawners.at("monster")[0].type == Identifier::vanilla("zombie"));
    CHECK(settings.spawners.at("monster")[0].weight == 95);
    CHECK(settings.spawners.at("monster")[0].minCount == 4);
    CHECK(settings.spawners.at("monster")[1].type == Identifier::vanilla("skeleton"));
    CHECK(settings.spawners.at("creature").size() == 1);
    CHECK(settings.spawners.at("ambient").empty());
}

TEST_CASE("the creature probability is the biome's own, or vanilla's default") {
    CHECK(RealBiomeSpawns::parse(kPlains).creatureProbability == doctest::Approx(0.07F));

    // parity: `MobSpawnSettings.creatureGenerationProbability` defaults to a tenth.
    CHECK(RealBiomeSpawns::parse(R"({"temperature": 0.5})").creatureProbability ==
          doctest::Approx(RealBiomeSpawns::kDefaultCreatureProbability));
}

TEST_CASE("a spawn cost is read as its two numbers") {
    const RealBiomeSpawns::Settings settings = RealBiomeSpawns::parse(kPlains);

    REQUIRE(settings.costs.contains(Identifier::vanilla("magma_cube")));
    const MobSpawnCost cost = settings.costs.at(Identifier::vanilla("magma_cube"));
    CHECK(cost.energyBudget == doctest::Approx(0.7));
    CHECK(cost.charge == doctest::Approx(0.15));
    CHECK_FALSE(settings.costs.contains(Identifier::vanilla("zombie")));
}

TEST_CASE("an entry missing any of its four fields is left out rather than guessed at") {
    const RealBiomeSpawns::Settings settings = RealBiomeSpawns::parse(R"({
      "spawners": {"monster": [
        {"type": "minecraft:zombie", "weight": 1, "minCount": 1, "maxCount": 1},
        {"type": "minecraft:creeper", "weight": 1, "minCount": 1},
        {"weight": 1, "minCount": 1, "maxCount": 1},
        {"type": "one:too:many", "weight": 1, "minCount": 1, "maxCount": 1},
        7
      ]}
    })");

    REQUIRE(settings.spawners.at("monster").size() == 1);
    CHECK(settings.spawners.at("monster")[0].type == Identifier::vanilla("zombie"));
}

TEST_CASE("a file that does not parse offers nothing at all") {
    const RealBiomeSpawns::Settings broken = RealBiomeSpawns::parse("{\"spawners\": {");

    CHECK(broken.spawners.empty());
    CHECK(broken.costs.empty());
    CHECK(broken.creatureProbability == doctest::Approx(RealBiomeSpawns::kDefaultCreatureProbability));

    // And so does one that is not an object.
    CHECK(RealBiomeSpawns::parse("[]").spawners.empty());
}

TEST_CASE("a biome nobody wrote a file for offers nothing") {
    const RealBiomeSpawns spawns{
        RealBiomeSpawns::Jar{.readText = [](std::string_view) { return std::optional<std::string>{}; }}};

    CHECK(spawns.listFor(Identifier::vanilla("plains"), "monster").empty());
    CHECK(spawns.creatureProbabilityFor(Identifier::vanilla("plains")) ==
          doctest::Approx(RealBiomeSpawns::kDefaultCreatureProbability));
    CHECK_FALSE(spawns.costFor(Identifier::vanilla("plains"), Identifier::vanilla("zombie")).has_value());
}

TEST_CASE("a biome is read from the jar once and answered from the cache after that") {
    std::int32_t reads = 0;
    const RealBiomeSpawns spawns{RealBiomeSpawns::Jar{.readText = [&reads](std::string_view name) {
        reads++;
        CHECK(name == "data/minecraft/worldgen/biome/plains.json");
        return std::optional<std::string>{std::string{kPlains}};
    }}};

    CHECK(spawns.listFor(Identifier::vanilla("plains"), "monster").size() == 2);
    CHECK(spawns.listFor(Identifier::vanilla("plains"), "creature").size() == 1);
    CHECK(spawns.creatureProbabilityFor(Identifier::vanilla("plains")) == doctest::Approx(0.07F));
    REQUIRE(spawns.costFor(Identifier::vanilla("plains"), Identifier::vanilla("magma_cube")).has_value());
    CHECK(reads == 1);
}

TEST_CASE("a biome is looked up by its path, whatever namespace it came from") {
    const RealBiomeSpawns spawns{RealBiomeSpawns::Jar{
        .readText = [](std::string_view) { return std::optional<std::string>{std::string{kPlains}}; }}};

    CHECK(spawns.of(Identifier{"basalt", "plains"}).spawners.size() == 3);
    CHECK(spawns.listFor(Identifier::vanilla("plains"), "nothing_of_the_sort").empty());
}

TEST_CASE("a 26.3 biome keeps its spawns in environment attributes") {
    // Shaped like the jar's `soul_sand_valley`: a constant and a uniform `count`.
    const RealBiomeSpawns::Settings settings = RealBiomeSpawns::parse(R"({
      "attributes": {
        "minecraft:gameplay/creature_world_gen_spawn_probability": 0.04,
        "minecraft:gameplay/natural_mob_spawns": {"modifier": "overlay", "argument": {
          "spawn_costs": {"minecraft:ghast": {"charge": 0.7, "energy_budget": 0.15}},
          "spawns_by_category": {
            "creature": [{"type": "minecraft:strider", "count": {"type": "minecraft:uniform",
                          "max_inclusive": 2, "min_inclusive": 1}, "weight": 60}],
            "monster": [{"type": "minecraft:ghast", "count": 4, "weight": 50}]
          }}}
      }
    })");

    CHECK(settings.creatureProbability == doctest::Approx(0.04F));
    REQUIRE(settings.spawners.at("creature").size() == 1);
    const SpawnerEntry& strider = settings.spawners.at("creature")[0];
    CHECK(strider.minCount == 1);
    CHECK(strider.maxCount == 2);
    CHECK_FALSE(strider.constantCount);
    REQUIRE(settings.spawners.at("monster").size() == 1);
    const SpawnerEntry& ghast = settings.spawners.at("monster")[0];
    CHECK(ghast.minCount == 4);
    CHECK(ghast.maxCount == 4);
    CHECK(ghast.constantCount);
    CHECK(settings.costs.contains(Identifier::vanilla("ghast")));
}
