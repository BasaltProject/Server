#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/structure/MineshaftBlocks.hpp>
#include <basalt/server/world/structure/StructureSpawnOverrides.hpp>

#include <doctest.h>

#include <array>
#include <string>
#include <string_view>

using namespace basalt::server::world::structure;
namespace overrides = basalt::server::world::structure::structureSpawnOverrides;

namespace {

constexpr std::array<std::string_view, 8> kCategories{
    "monster",        "creature",      "ambient", "axolotls", "underground_water_creature",
    "water_creature", "water_ambient", "misc"};

} // namespace

TEST_CASE("the structure spawn overrides match the frozen kotlin") {
    std::string structures;
    for (const std::string& structure : overrides::structures()) {
        if (!structures.empty()) {
            structures += ",";
        }
        structures += structure;
    }
    CHECK(structures == "ancient_city,fortress,monument,pillager_outpost,swamp_hut");

    std::string rows;
    for (const std::string& structure : overrides::structures()) {
        for (const std::string_view category : kCategories) {
            const overrides::Override* override = overrides::of(structure, category);
            if (override == nullptr) {
                continue;
            }
            rows += structure + "." + std::string{category} + "=";
            rows += override->bounds == overrides::Bounds::Piece ? "PIECE:" : "FULL:";
            bool first = true;
            for (const overrides::SpawnEntry& entry : override->spawns) {
                if (!first) {
                    rows += ",";
                }
                first = false;
                rows += entry.type + "/" + std::to_string(entry.weight) + "/" + std::to_string(entry.minCount) + "/" +
                        std::to_string(entry.maxCount);
            }
            rows += ";";
        }
    }
    CHECK(rows == "ancient_city.monster=FULL:;ancient_city.creature=FULL:;ancient_city.ambient=FULL:;ancient_city.axolo"
                  "tls=FULL:;ancient_city.underground_water_creature=FULL:;ancient_city.water_creature=FULL:;ancient_ci"
                  "ty.water_ambient=FULL:;ancient_city.misc=FULL:;fortress.monster=PIECE:minecraft:blaze/10/2/3,minecra"
                  "ft:zombified_piglin/5/4/4,minecraft:wither_skeleton/8/5/5,minecraft:skeleton/2/5/5,minecraft:magma_c"
                  "ube/3/4/4;monument.monster=FULL:minecraft:guardian/1/2/4;monument.axolotls=FULL:;monument.undergroun"
                  "d_water_creature=FULL:;pillager_outpost.monster=FULL:minecraft:pillager/1/1/1;swamp_hut.monster=PIEC"
                  "E:minecraft:witch/1/1/1;swamp_hut.creature=PIECE:minecraft:cat/1/1/1;");

    CHECK(overrides::of("igloo", "monster") == nullptr);
    CHECK(overrides::of("fortress", "misc") == nullptr);
}

TEST_CASE("the mineshaft block set matches the frozen kotlin") {
    const MineshaftBlocks normal{MineshaftType::Normal};
    CHECK(normal.log() == 137);
    CHECK(normal.planks() == 15);
    CHECK(normal.fence() == 6996);
    CHECK(normal.fenceWest() == 6995);
    CHECK(normal.fenceEast() == 6980);

    const MineshaftBlocks mesa{MineshaftType::Mesa};
    CHECK(mesa.log() == 155);
    CHECK(mesa.planks() == 21);
    CHECK(mesa.fence() == 13963);
    CHECK(mesa.fenceWest() == 13962);
    CHECK(mesa.fenceEast() == 13947);
}
