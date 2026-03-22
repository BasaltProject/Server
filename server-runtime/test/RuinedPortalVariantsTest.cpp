#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/BiomeTagLoader.hpp>
#include <basalt/server/runtime/world/RuinedPortalVariants.hpp>

#include <doctest.h>

#include <map>
#include <string>

using basalt::server::runtime::world::BiomeTagLoader;
using basalt::server::world::terrain::RuinedPortalPlacement;
using basalt::server::world::terrain::RuinedPortalVariant;
namespace variants = basalt::server::runtime::world::ruinedPortalVariants;

namespace {

/// A jar whose `has_structure` tags name one biome each, so the variant they landed on is visible.
[[nodiscard]] BiomeTagLoader::ReadEntry tagReader(const std::map<std::string, std::string>& files) {
    return [files](std::string_view name) -> std::optional<std::string> {
        const auto found = files.find(std::string{name});
        return found == files.end() ? std::nullopt : std::optional{found->second};
    };
}

[[nodiscard]] std::string tagFor(const std::string& structure) {
    return "data/minecraft/tags/worldgen/biome/has_structure/" + structure + ".json";
}

[[nodiscard]] const RuinedPortalVariant& named(const std::vector<RuinedPortalVariant>& all, const std::string& name) {
    for (const RuinedPortalVariant& variant : all) {
        if (variant.name == name) {
            return variant;
        }
    }
    REQUIRE_MESSAGE(false, "no variant called " << name);
    return all.front();
}

} // namespace

TEST_CASE("the seven Overworld variants, each with its own placement") {
    BiomeTagLoader tags{tagReader({})};

    const std::vector<RuinedPortalVariant> all = variants::overworld(tags);

    REQUIRE(all.size() == 7);
    CHECK(all[0].name == "ruined_portal");
    CHECK(all[1].name == "ruined_portal_desert");
    CHECK(all[2].name == "ruined_portal_jungle");
    CHECK(all[3].name == "ruined_portal_swamp");
    CHECK(all[4].name == "ruined_portal_mountain");
    CHECK(all[5].name == "ruined_portal_ocean");
    CHECK(all[6].name == "ruined_portal_nether");
}

TEST_CASE("the two variants that pick between two placements weigh them evenly") {
    BiomeTagLoader tags{tagReader({})};
    const std::vector<RuinedPortalVariant> all = variants::overworld(tags);

    for (const std::string& name : {std::string{"ruined_portal"}, std::string{"ruined_portal_mountain"}}) {
        const RuinedPortalVariant& variant = named(all, name);
        REQUIRE(variant.setups.size() == 2);
        CHECK(variant.setups[0].weight == doctest::Approx(0.5F));
        CHECK(variant.setups[1].weight == doctest::Approx(0.5F));
        CHECK(variant.setups[1].placement == RuinedPortalPlacement::OnLandSurface);
        CHECK(variant.setups[1].airPocketProbability == doctest::Approx(0.0F));
        CHECK(variant.setups[0].airPocketProbability == doctest::Approx(1.0F));
        CHECK(variant.setups[0].canBeCold);
        CHECK(variant.setups[1].canBeCold);
        CHECK(variant.setups[1].mossiness == doctest::Approx(0.2F));
        CHECK(variant.setups[0].mossiness == doctest::Approx(0.2F));
    }
    CHECK(named(all, "ruined_portal").setups[0].placement == RuinedPortalPlacement::Underground);
    CHECK(named(all, "ruined_portal_mountain").setups[0].placement == RuinedPortalPlacement::InMountain);
}

TEST_CASE("each of the five single-placement variants keeps its own numbers") {
    BiomeTagLoader tags{tagReader({})};
    const std::vector<RuinedPortalVariant> all = variants::overworld(tags);

    const auto only = [&all](const std::string& name) {
        const RuinedPortalVariant& variant = named(all, name);
        REQUIRE(variant.setups.size() == 1);
        return variant.setups[0];
    };

    const auto desert = only("ruined_portal_desert");
    CHECK(desert.placement == RuinedPortalPlacement::PartlyBuried);
    CHECK(desert.airPocketProbability == doctest::Approx(0.0F));
    CHECK(desert.mossiness == doctest::Approx(0.0F));
    CHECK(desert.weight == doctest::Approx(1.0F));
    CHECK_FALSE(desert.canBeCold);

    const auto jungle = only("ruined_portal_jungle");
    CHECK(jungle.placement == RuinedPortalPlacement::OnLandSurface);
    CHECK(jungle.mossiness == doctest::Approx(0.8F));
    CHECK(jungle.overgrown);
    CHECK(jungle.vines);

    const auto swamp = only("ruined_portal_swamp");
    CHECK(swamp.placement == RuinedPortalPlacement::OnOceanFloor);
    CHECK(swamp.mossiness == doctest::Approx(0.5F));
    CHECK(swamp.vines);
    CHECK_FALSE(swamp.overgrown);
    CHECK(swamp.airPocketProbability == doctest::Approx(0.0F));

    const auto ocean = only("ruined_portal_ocean");
    CHECK(ocean.placement == RuinedPortalPlacement::OnOceanFloor);
    CHECK(ocean.mossiness == doctest::Approx(0.8F));
    CHECK(ocean.canBeCold);
    CHECK_FALSE(ocean.vines);

    const auto nether = only("ruined_portal_nether");
    CHECK(nether.placement == RuinedPortalPlacement::InNether);
    CHECK(nether.airPocketProbability == doctest::Approx(0.0F));
    CHECK(nether.replaceWithBlackstone);
    CHECK(nether.mossiness == doctest::Approx(0.0F));
    CHECK_FALSE(only("ruined_portal_desert").replaceWithBlackstone);
}

TEST_CASE("each variant takes its biomes from its own has_structure tag") {
    BiomeTagLoader tags{tagReader({
        {tagFor("ruined_portal_standard"), R"({"values": ["minecraft:plains", "minecraft:forest"]})"},
        {tagFor("ruined_portal_desert"), R"({"values": ["minecraft:desert"]})"},
    })};

    const std::vector<RuinedPortalVariant> all = variants::overworld(tags);

    CHECK(named(all, "ruined_portal").biomes ==
          std::set<std::string, std::less<>>{"minecraft:plains", "minecraft:forest"});
    CHECK(named(all, "ruined_portal_desert").biomes == std::set<std::string, std::less<>>{"minecraft:desert"});
    // A tag the jar does not ship leaves that variant with nowhere to start.
    CHECK(named(all, "ruined_portal_jungle").biomes.empty());
}
