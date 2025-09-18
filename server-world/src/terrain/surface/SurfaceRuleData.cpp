#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleData.hpp>

#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::surface {
namespace {

namespace rules = surfaceRules;

[[nodiscard]] RuleSource namedState(std::string_view name) {
    const registry::BlockState* found = registry::blocks::named(name);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{name} + "' while building surface rules"};
    }
    return rules::state(found->stateId);
}

[[nodiscard]] ConditionSource surfaceNoiseAbove(double threshold) {
    return rules::noiseCondition2d("surface", threshold / 8.25, std::numeric_limits<double>::max());
}

/// Every block the two trees name, resolved once so the tree itself stays readable.
struct States {
    RuleSource air = rules::state(registry::blocks::air().stateId);
    RuleSource bedrock = rules::state(registry::blocks::bedrock().stateId);
    RuleSource whiteTerracotta = namedState("white_terracotta");
    RuleSource orangeTerracotta = namedState("orange_terracotta");
    RuleSource terracotta = namedState("terracotta");
    RuleSource redSand = namedState("red_sand");
    RuleSource redSandstone = namedState("red_sandstone");
    RuleSource stone = rules::state(registry::blocks::stone().stateId);
    RuleSource deepslate = namedState("deepslate");
    RuleSource dirt = namedState("dirt");
    RuleSource podzol = namedState("podzol");
    RuleSource coarseDirt = namedState("coarse_dirt");
    RuleSource mycelium = namedState("mycelium");
    RuleSource grassBlock = namedState("grass_block");
    RuleSource calcite = namedState("calcite");
    RuleSource gravel = namedState("gravel");
    RuleSource sand = namedState("sand");
    RuleSource sandstone = namedState("sandstone");
    RuleSource packedIce = namedState("packed_ice");
    RuleSource snowBlock = namedState("snow_block");
    RuleSource mud = namedState("mud");
    RuleSource powderSnow = namedState("powder_snow");
    RuleSource ice = namedState("ice");
    RuleSource water = namedState("water");
    RuleSource cinnabar = namedState("cinnabar");
    RuleSource sulfur = namedState("sulfur");
    RuleSource lava = rules::state(registry::blocks::lava().stateId);
    RuleSource netherrack = namedState("netherrack");
    RuleSource soulSand = namedState("soul_sand");
    RuleSource soulSoil = namedState("soul_soil");
    RuleSource basalt = namedState("basalt");
    RuleSource blackstone = namedState("blackstone");
    RuleSource warpedWartBlock = namedState("warped_wart_block");
    RuleSource warpedNylium = namedState("warped_nylium");
    RuleSource netherWartBlock = namedState("nether_wart_block");
    RuleSource crimsonNylium = namedState("crimson_nylium");
};

[[nodiscard]] const States& states() {
    static const States built;
    return built;
}

[[nodiscard]] RuleSource buildNether() {
    const States& s = states();
    const ConditionSource aboveLavaLevel = rules::yBlockCheck(VerticalAnchor::absolute(31), 0);
    const ConditionSource aboveLavaSurface = rules::yBlockCheck(VerticalAnchor::absolute(32), 0);
    const ConditionSource aroundLavaBottom = rules::yStartCheck(VerticalAnchor::absolute(30), 0);
    const ConditionSource aroundLavaTop = rules::notCondition(rules::yStartCheck(VerticalAnchor::absolute(35), 0));
    const ConditionSource closeToCeiling = rules::yBlockCheck(VerticalAnchor::belowTop(5), 0);
    const ConditionSource soulSandLayer = rules::noiseCondition2d("soul_sand_layer", -0.012);
    const ConditionSource gravelLayer = rules::noiseCondition2d("gravel_layer", -0.012);
    const ConditionSource patch = rules::noiseCondition2d("patch", -0.012);
    const ConditionSource netherrackNoise = rules::noiseCondition2d("netherrack", 0.54);
    const ConditionSource netherWart = rules::noiseCondition2d("nether_wart", 1.17);
    const ConditionSource stateSelector = rules::noiseCondition2d("nether_state_selector", 0.0);
    const RuleSource gravelPatch =
        rules::ifTrue(patch, rules::ifTrue(aroundLavaBottom, rules::ifTrue(aroundLavaTop, s.gravel)));

    return rules::sequence({
        rules::ifTrue(
            rules::verticalGradient("bedrock_floor", VerticalAnchor::bottom(), VerticalAnchor::aboveBottom(5)),
            s.bedrock),
        rules::ifTrue(rules::notCondition(
                          rules::verticalGradient("bedrock_roof", VerticalAnchor::belowTop(5), VerticalAnchor::top())),
                      s.bedrock),
        rules::ifTrue(closeToCeiling, s.netherrack),
        rules::ifTrue(
            rules::isBiome({"minecraft:basalt_deltas"}),
            rules::sequence({
                rules::ifTrue(rules::underCeiling(), s.basalt),
                rules::ifTrue(rules::underFloor(),
                              rules::sequence({gravelPatch, rules::ifTrue(stateSelector, s.basalt), s.blackstone})),
            })),
        rules::ifTrue(
            rules::isBiome({"minecraft:soul_sand_valley"}),
            rules::sequence({
                rules::ifTrue(rules::underCeiling(),
                              rules::sequence({rules::ifTrue(stateSelector, s.soulSand), s.soulSoil})),
                rules::ifTrue(rules::underFloor(),
                              rules::sequence({gravelPatch, rules::ifTrue(stateSelector, s.soulSand), s.soulSoil})),
            })),
        rules::ifTrue(
            rules::onFloor(),
            rules::sequence({
                rules::ifTrue(rules::notCondition(aboveLavaSurface), rules::ifTrue(rules::hole(), s.lava)),
                rules::ifTrue(rules::isBiome({"minecraft:warped_forest"}),
                              rules::ifTrue(rules::notCondition(netherrackNoise),
                                            rules::ifTrue(aboveLavaLevel,
                                                          rules::sequence({rules::ifTrue(netherWart, s.warpedWartBlock),
                                                                           s.warpedNylium})))),
                rules::ifTrue(rules::isBiome({"minecraft:crimson_forest"}),
                              rules::ifTrue(rules::notCondition(netherrackNoise),
                                            rules::ifTrue(aboveLavaLevel,
                                                          rules::sequence({rules::ifTrue(netherWart, s.netherWartBlock),
                                                                           s.crimsonNylium})))),
            })),
        rules::ifTrue(
            rules::isBiome({"minecraft:nether_wastes"}),
            rules::sequence({
                rules::ifTrue(rules::underFloor(),
                              rules::ifTrue(soulSandLayer,
                                            rules::sequence(
                                                {rules::ifTrue(rules::notCondition(rules::hole()),
                                                               rules::ifTrue(aroundLavaBottom,
                                                                             rules::ifTrue(aroundLavaTop, s.soulSand))),
                                                 s.netherrack}))),
                rules::ifTrue(
                    rules::onFloor(),
                    rules::ifTrue(
                        aboveLavaLevel,
                        rules::ifTrue(aroundLavaTop,
                                      rules::ifTrue(gravelLayer,
                                                    rules::sequence({rules::ifTrue(aboveLavaSurface, s.gravel),
                                                                     rules::ifTrue(rules::notCondition(rules::hole()),
                                                                                   s.gravel)}))))),
            })),
        s.netherrack,
    });
}

} // namespace

RuleSource surfaceRuleData::overworldLike(bool doPreliminarySurfaceCheck, bool bedrockRoof, bool bedrockFloor) {
    const States& s = states();
    const ConditionSource woodedBadlandsTop = rules::yBlockCheck(VerticalAnchor::absolute(97), 2);
    const ConditionSource badlandsTop = rules::yBlockCheck(VerticalAnchor::absolute(256), 0);
    const ConditionSource badlandsHeightCondition = rules::yStartCheck(VerticalAnchor::absolute(63), -1);
    const ConditionSource badlandsMid = rules::yStartCheck(VerticalAnchor::absolute(74), 1);
    const ConditionSource mangroveSwampPuddleLevel = rules::yBlockCheck(VerticalAnchor::absolute(60), 0);
    const ConditionSource swampPuddleLevel = rules::yBlockCheck(VerticalAnchor::absolute(62), 0);
    const ConditionSource aboveOverworldSeaLevel = rules::yBlockCheck(VerticalAnchor::absolute(63), 0);
    const ConditionSource notUnderwater = rules::waterBlockCheck(-1, 0);
    const ConditionSource aboveWater = rules::waterBlockCheck(0, 0);
    const ConditionSource notUnderDeepWater = rules::waterStartCheck(-6, -1);
    const ConditionSource frozenOcean = rules::isBiome({"minecraft:frozen_ocean", "minecraft:deep_frozen_ocean"});
    const ConditionSource steepCondition = rules::steep();
    const RuleSource grassOrDirtIfUnderwater = rules::sequence({rules::ifTrue(aboveWater, s.grassBlock), s.dirt});
    const RuleSource sandOrSandstoneIfCeiling =
        rules::sequence({rules::ifTrue(rules::onCeiling(), s.sandstone), s.sand});
    const RuleSource gravelOrStoneIfCeiling = rules::sequence({rules::ifTrue(rules::onCeiling(), s.stone), s.gravel});
    const ConditionSource biomesWithSandAndSandstone =
        rules::isBiome({"minecraft:warm_ocean", "minecraft:beach", "minecraft:snowy_beach"});
    const ConditionSource biomesWithSandAndVeryDeepSandstone = rules::isBiome({"minecraft:desert"});
    // parity: these three read the noise in 3d, unlike every other band here, and their bounds are
    // float literals in the jar, so they widen to a double a hair off the round number.
    const RuleSource sulfurCaveBands = rules::sequence({
        rules::ifTrue(rules::noiseCondition3d("sulfur_cave_gradient", -0.40000000596046448, -0.10000000149011612),
                      s.cinnabar),
        rules::ifTrue(rules::noiseCondition3d("sulfur_cave_gradient", 0.0, 0.40000000596046448), s.sulfur),
        rules::ifTrue(rules::noiseCondition3d("sulfur_cave_gradient", 0.40000000596046448), s.cinnabar),
    });
    const RuleSource commonSurfaceAndUnderRules = rules::sequence({
        rules::ifTrue(
            rules::isBiome({"minecraft:stony_peaks"}),
            rules::sequence({rules::ifTrue(rules::noiseCondition2d("calcite", -0.0125, 0.0125), s.calcite), s.stone})),
        rules::ifTrue(
            rules::isBiome({"minecraft:stony_shore"}),
            rules::sequence(
                {rules::ifTrue(rules::noiseCondition2d("gravel", -0.05, 0.05), gravelOrStoneIfCeiling), s.stone})),
        rules::ifTrue(rules::isBiome({"minecraft:windswept_hills"}), rules::ifTrue(surfaceNoiseAbove(1.0), s.stone)),
        rules::ifTrue(biomesWithSandAndSandstone, sandOrSandstoneIfCeiling),
        rules::ifTrue(biomesWithSandAndVeryDeepSandstone, sandOrSandstoneIfCeiling),
        rules::ifTrue(rules::isBiome({"minecraft:dripstone_caves"}), s.stone),
        rules::ifTrue(rules::isBiome({"minecraft:sulfur_caves"}), rules::sequence({sulfurCaveBands, s.stone})),
    });
    const RuleSource powderSnowUnderRule =
        rules::ifTrue(rules::noiseCondition2d("powder_snow", 0.45, 0.58), rules::ifTrue(aboveWater, s.powderSnow));
    const RuleSource powderSnowSurfaceRule =
        rules::ifTrue(rules::noiseCondition2d("powder_snow", 0.35, 0.6), rules::ifTrue(aboveWater, s.powderSnow));
    const RuleSource biomeUnderSurfaceRule = rules::sequence({
        rules::ifTrue(rules::isBiome({"minecraft:frozen_peaks"}),
                      rules::sequence({
                          rules::ifTrue(steepCondition, s.packedIce),
                          rules::ifTrue(rules::noiseCondition2d("packed_ice", -0.5, 0.2), s.packedIce),
                          rules::ifTrue(rules::noiseCondition2d("ice", -0.0625, 0.025), s.ice),
                          rules::ifTrue(aboveWater, s.snowBlock),
                      })),
        rules::ifTrue(rules::isBiome({"minecraft:snowy_slopes"}),
                      rules::sequence({rules::ifTrue(steepCondition, s.stone), powderSnowUnderRule,
                                       rules::ifTrue(aboveWater, s.snowBlock)})),
        rules::ifTrue(rules::isBiome({"minecraft:jagged_peaks"}), s.stone),
        rules::ifTrue(rules::isBiome({"minecraft:grove"}), rules::sequence({powderSnowUnderRule, s.dirt})),
        commonSurfaceAndUnderRules,
        rules::ifTrue(rules::isBiome({"minecraft:windswept_savanna"}), rules::ifTrue(surfaceNoiseAbove(1.75), s.stone)),
        rules::ifTrue(rules::isBiome({"minecraft:windswept_gravelly_hills"}),
                      rules::sequence({
                          rules::ifTrue(surfaceNoiseAbove(2.0), gravelOrStoneIfCeiling),
                          rules::ifTrue(surfaceNoiseAbove(1.0), s.stone),
                          rules::ifTrue(surfaceNoiseAbove(-1.0), s.dirt),
                          gravelOrStoneIfCeiling,
                      })),
        rules::ifTrue(rules::isBiome({"minecraft:mangrove_swamp"}), s.mud),
        s.dirt,
    });
    const RuleSource biomeSurfaceRule = rules::sequence({
        rules::ifTrue(rules::isBiome({"minecraft:frozen_peaks"}),
                      rules::sequence({
                          rules::ifTrue(steepCondition, s.packedIce),
                          rules::ifTrue(rules::noiseCondition2d("packed_ice", 0.0, 0.2), s.packedIce),
                          rules::ifTrue(rules::noiseCondition2d("ice", 0.0, 0.025), s.ice),
                          rules::ifTrue(aboveWater, s.snowBlock),
                      })),
        rules::ifTrue(rules::isBiome({"minecraft:snowy_slopes"}),
                      rules::sequence({rules::ifTrue(steepCondition, s.stone), powderSnowSurfaceRule,
                                       rules::ifTrue(aboveWater, s.snowBlock)})),
        rules::ifTrue(
            rules::isBiome({"minecraft:jagged_peaks"}),
            rules::sequence({rules::ifTrue(steepCondition, s.stone), rules::ifTrue(aboveWater, s.snowBlock)})),
        rules::ifTrue(rules::isBiome({"minecraft:grove"}),
                      rules::sequence({powderSnowSurfaceRule, rules::ifTrue(aboveWater, s.snowBlock)})),
        commonSurfaceAndUnderRules,
        rules::ifTrue(rules::isBiome({"minecraft:windswept_savanna"}),
                      rules::sequence({rules::ifTrue(surfaceNoiseAbove(1.75), s.stone),
                                       rules::ifTrue(surfaceNoiseAbove(-0.5), s.coarseDirt)})),
        rules::ifTrue(rules::isBiome({"minecraft:windswept_gravelly_hills"}),
                      rules::sequence({
                          rules::ifTrue(surfaceNoiseAbove(2.0), gravelOrStoneIfCeiling),
                          rules::ifTrue(surfaceNoiseAbove(1.0), s.stone),
                          rules::ifTrue(surfaceNoiseAbove(-1.0), grassOrDirtIfUnderwater),
                          gravelOrStoneIfCeiling,
                      })),
        rules::ifTrue(rules::isBiome({"minecraft:old_growth_pine_taiga", "minecraft:old_growth_spruce_taiga"}),
                      rules::sequence({rules::ifTrue(surfaceNoiseAbove(1.75), s.coarseDirt),
                                       rules::ifTrue(surfaceNoiseAbove(-0.95), s.podzol)})),
        rules::ifTrue(rules::isBiome({"minecraft:ice_spikes"}), rules::ifTrue(aboveWater, s.snowBlock)),
        rules::ifTrue(rules::isBiome({"minecraft:mangrove_swamp"}), s.mud),
        rules::ifTrue(rules::isBiome({"minecraft:mushroom_fields"}), s.mycelium),
        grassOrDirtIfUnderwater,
    });
    const ConditionSource clayBand1 = rules::noiseCondition2d("surface", -0.909, -0.5454);
    const ConditionSource clayBand2 = rules::noiseCondition2d("surface", -0.1818, 0.1818);
    const ConditionSource clayBand3 = rules::noiseCondition2d("surface", 0.5454, 0.909);
    const RuleSource mainRuleCloseToSurface = rules::sequence({
        rules::ifTrue(
            rules::onFloor(),
            rules::sequence({
                rules::ifTrue(rules::isBiome({"minecraft:wooded_badlands"}),
                              rules::ifTrue(woodedBadlandsTop, rules::sequence({rules::ifTrue(clayBand1, s.coarseDirt),
                                                                                rules::ifTrue(clayBand2, s.coarseDirt),
                                                                                rules::ifTrue(clayBand3, s.coarseDirt),
                                                                                grassOrDirtIfUnderwater}))),
                rules::ifTrue(rules::isBiome({"minecraft:swamp"}),
                              rules::ifTrue(swampPuddleLevel,
                                            rules::ifTrue(rules::notCondition(aboveOverworldSeaLevel),
                                                          rules::ifTrue(rules::noiseCondition2d("surface_swamp", 0.0),
                                                                        s.water)))),
                rules::ifTrue(rules::isBiome({"minecraft:mangrove_swamp"}),
                              rules::ifTrue(mangroveSwampPuddleLevel,
                                            rules::ifTrue(rules::notCondition(aboveOverworldSeaLevel),
                                                          rules::ifTrue(rules::noiseCondition2d("surface_swamp", 0.0),
                                                                        s.water)))),
            })),
        rules::ifTrue(
            rules::isBiome({"minecraft:badlands", "minecraft:eroded_badlands", "minecraft:wooded_badlands"}),
            rules::sequence({
                rules::ifTrue(
                    rules::onFloor(),
                    rules::sequence({
                        rules::ifTrue(badlandsTop, s.orangeTerracotta),
                        rules::ifTrue(badlandsMid,
                                      rules::sequence({rules::ifTrue(clayBand1, s.terracotta),
                                                       rules::ifTrue(clayBand2, s.terracotta),
                                                       rules::ifTrue(clayBand3, s.terracotta), rules::bandlands()})),
                        rules::ifTrue(notUnderwater,
                                      rules::sequence({rules::ifTrue(rules::onCeiling(), s.redSandstone), s.redSand})),
                        rules::ifTrue(rules::notCondition(rules::hole()), s.orangeTerracotta),
                        rules::ifTrue(notUnderDeepWater, s.whiteTerracotta),
                        gravelOrStoneIfCeiling,
                    })),
                rules::ifTrue(
                    badlandsHeightCondition,
                    rules::sequence({rules::ifTrue(aboveOverworldSeaLevel,
                                                   rules::ifTrue(rules::notCondition(badlandsMid), s.orangeTerracotta)),
                                     rules::bandlands()})),
                rules::ifTrue(rules::underFloor(), rules::ifTrue(notUnderDeepWater, s.whiteTerracotta)),
            })),
        rules::ifTrue(
            rules::onFloor(),
            rules::ifTrue(notUnderwater,
                          rules::sequence({
                              rules::ifTrue(frozenOcean,
                                            rules::ifTrue(rules::hole(),
                                                          rules::sequence({rules::ifTrue(aboveWater, s.air),
                                                                           rules::ifTrue(rules::temperature(), s.ice),
                                                                           s.water}))),
                              biomeSurfaceRule,
                          }))),
        rules::ifTrue(
            notUnderDeepWater,
            rules::sequence({
                rules::ifTrue(rules::onFloor(), rules::ifTrue(frozenOcean, rules::ifTrue(rules::hole(), s.water))),
                rules::ifTrue(rules::underFloor(), biomeUnderSurfaceRule),
                rules::ifTrue(biomesWithSandAndSandstone, rules::ifTrue(rules::deepUnderFloor(), s.sandstone)),
                rules::ifTrue(biomesWithSandAndVeryDeepSandstone,
                              rules::ifTrue(rules::veryDeepUnderFloor(), s.sandstone)),
            })),
        rules::ifTrue(rules::onFloor(),
                      rules::sequence({
                          rules::ifTrue(rules::isBiome({"minecraft:frozen_peaks", "minecraft:jagged_peaks"}), s.stone),
                          rules::ifTrue(rules::isBiome({"minecraft:warm_ocean", "minecraft:lukewarm_ocean",
                                                        "minecraft:deep_lukewarm_ocean"}),
                                        sandOrSandstoneIfCeiling),
                          gravelOrStoneIfCeiling,
                      })),
    });

    std::vector<RuleSource> built;
    if (bedrockRoof) {
        built.push_back(rules::ifTrue(rules::notCondition(rules::verticalGradient(
                                          "bedrock_roof", VerticalAnchor::belowTop(5), VerticalAnchor::top())),
                                      s.bedrock));
    }
    if (bedrockFloor) {
        built.push_back(rules::ifTrue(
            rules::verticalGradient("bedrock_floor", VerticalAnchor::bottom(), VerticalAnchor::aboveBottom(5)),
            s.bedrock));
    }
    built.push_back(doPreliminarySurfaceCheck ? rules::ifTrue(rules::abovePreliminarySurface(), mainRuleCloseToSurface)
                                              : mainRuleCloseToSurface);
    built.push_back(rules::ifTrue(rules::isBiome({"minecraft:sulfur_caves"}), sulfurCaveBands));
    built.push_back(rules::ifTrue(
        rules::verticalGradient("deepslate", VerticalAnchor::absolute(0), VerticalAnchor::absolute(8)), s.deepslate));
    return rules::sequence(std::move(built));
}

const RuleSource& surfaceRuleData::overworld() {
    static const RuleSource cached = overworldLike(true, false, true);
    return cached;
}

const RuleSource& surfaceRuleData::nether() {
    static const RuleSource cached = buildNether();
    return cached;
}

} // namespace basalt::server::world::terrain::surface
