#include <basalt/server/runtime/world/BiomeTagLoader.hpp>
#include <basalt/server/runtime/world/RuinedPortalVariants.hpp>

#include <string>

namespace basalt::server::runtime::world::ruinedPortalVariants {
namespace {

using server::world::terrain::RuinedPortalPlacement;
using server::world::terrain::RuinedPortalSetup;
using server::world::terrain::RuinedPortalVariant;

[[nodiscard]] RuinedPortalVariant variant(std::string name, BiomeTagLoader& tags, const std::string& tag,
                                          std::vector<RuinedPortalSetup> setups) {
    const std::set<std::string>& biomes = tags.biomesIn("minecraft:has_structure/" + tag);
    return RuinedPortalVariant{.name = std::move(name),
                               .biomes = std::set<std::string, std::less<>>{biomes.begin(), biomes.end()},
                               .setups = std::move(setups)};
}

} // namespace

std::vector<RuinedPortalVariant> overworld(BiomeTagLoader& tags) {
    return {
        variant("ruined_portal", tags, "ruined_portal_standard",
                {RuinedPortalSetup{.placement = RuinedPortalPlacement::Underground,
                                   .airPocketProbability = 1.0F,
                                   .mossiness = 0.2F,
                                   .canBeCold = true,
                                   .weight = 0.5F},
                 RuinedPortalSetup{.placement = RuinedPortalPlacement::OnLandSurface,
                                   .airPocketProbability = 0.5F,
                                   .mossiness = 0.2F,
                                   .canBeCold = true,
                                   .weight = 0.5F}}),
        variant("ruined_portal_desert", tags, "ruined_portal_desert",
                {RuinedPortalSetup{.placement = RuinedPortalPlacement::PartlyBuried}}),
        variant("ruined_portal_jungle", tags, "ruined_portal_jungle",
                {RuinedPortalSetup{.placement = RuinedPortalPlacement::OnLandSurface,
                                   .airPocketProbability = 0.5F,
                                   .mossiness = 0.8F,
                                   .overgrown = true,
                                   .vines = true}}),
        variant(
            "ruined_portal_swamp", tags, "ruined_portal_swamp",
            {RuinedPortalSetup{.placement = RuinedPortalPlacement::OnOceanFloor, .mossiness = 0.5F, .vines = true}}),
        variant("ruined_portal_mountain", tags, "ruined_portal_mountain",
                {RuinedPortalSetup{.placement = RuinedPortalPlacement::InMountain,
                                   .airPocketProbability = 1.0F,
                                   .mossiness = 0.2F,
                                   .canBeCold = true,
                                   .weight = 0.5F},
                 RuinedPortalSetup{.placement = RuinedPortalPlacement::OnLandSurface,
                                   .airPocketProbability = 0.5F,
                                   .mossiness = 0.2F,
                                   .canBeCold = true,
                                   .weight = 0.5F}}),
        variant("ruined_portal_ocean", tags, "ruined_portal_ocean",
                {RuinedPortalSetup{
                    .placement = RuinedPortalPlacement::OnOceanFloor, .mossiness = 0.8F, .canBeCold = true}}),
        variant("ruined_portal_nether", tags, "ruined_portal_nether",
                {RuinedPortalSetup{.placement = RuinedPortalPlacement::InNether,
                                   .airPocketProbability = 0.5F,
                                   .replaceWithBlackstone = true}}),
    };
}

} // namespace basalt::server::runtime::world::ruinedPortalVariants
