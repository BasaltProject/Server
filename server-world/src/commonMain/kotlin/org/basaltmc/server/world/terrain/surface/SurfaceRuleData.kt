package org.basaltmc.server.world.terrain.surface

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.terrain.surface.SurfaceRules.hole
import org.basaltmc.server.world.terrain.surface.SurfaceRules.ifTrue
import org.basaltmc.server.world.terrain.surface.SurfaceRules.isBiome
import org.basaltmc.server.world.terrain.surface.SurfaceRules.noiseCondition2d
import org.basaltmc.server.world.terrain.surface.SurfaceRules.noiseCondition3d
import org.basaltmc.server.world.terrain.surface.SurfaceRules.not
import org.basaltmc.server.world.terrain.surface.SurfaceRules.sequence
import org.basaltmc.server.world.terrain.surface.SurfaceRules.state
import org.basaltmc.server.world.terrain.surface.SurfaceRules.steep
import org.basaltmc.server.world.terrain.surface.SurfaceRules.waterBlockCheck
import org.basaltmc.server.world.terrain.surface.SurfaceRules.waterStartCheck
import org.basaltmc.server.world.terrain.surface.SurfaceRules.yBlockCheck
import org.basaltmc.server.world.terrain.surface.SurfaceRules.yStartCheck
import org.basaltmc.wire.common.Identifier

/**
 * // parity: net.minecraft.data.worldgen.SurfaceRuleData, the whole `overworldLike` tree rather
 * than a Plains-only shortcut. Plains is never named in it: it falls through every other biome's
 * `isBiome` check to the generic defaults, so the other branches have to be there to reach them.
 *
 * Not ported: [SurfaceRules.temperature] always reports false, so per-biome snow is missing.
 * Badlands clay bands come from the surface environment.
 */
public object SurfaceRuleData {
    private fun state(name: String): RuleSource {
        val stateId =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId
                ?: error("unknown block '$name' while building surface rules")
        return state(stateId)
    }

    private val AIR = state(Blocks.AIR.stateId)
    private val BEDROCK = state(Blocks.BEDROCK.stateId)
    private val WHITE_TERRACOTTA = state("white_terracotta")
    private val ORANGE_TERRACOTTA = state("orange_terracotta")
    private val TERRACOTTA = state("terracotta")
    private val RED_SAND = state("red_sand")
    private val RED_SANDSTONE = state("red_sandstone")
    private val STONE = state(Blocks.STONE.stateId)
    private val DEEPSLATE = state("deepslate")
    private val DIRT = state(Blocks.DIRT.stateId)
    private val PODZOL = state("podzol")
    private val COARSE_DIRT = state("coarse_dirt")
    private val MYCELIUM = state("mycelium")
    private val GRASS_BLOCK = state(Blocks.GRASS_BLOCK.stateId)
    private val CALCITE = state("calcite")
    private val GRAVEL = state("gravel")
    private val SAND = state("sand")
    private val SANDSTONE = state("sandstone")
    private val PACKED_ICE = state("packed_ice")
    private val SNOW_BLOCK = state("snow_block")
    private val MUD = state("mud")
    private val POWDER_SNOW = state("powder_snow")
    private val ICE = state("ice")
    private val WATER = state("water")
    private val CINNABAR = state("cinnabar")
    private val SULFUR = state("sulfur")
    private val LAVA = state(Blocks.LAVA.stateId)
    private val NETHERRACK = state(Blocks.NETHERRACK.stateId)
    private val SOUL_SAND = state("soul_sand")
    private val SOUL_SOIL = state("soul_soil")
    private val BASALT = state("basalt")
    private val BLACKSTONE = state("blackstone")
    private val WARPED_WART_BLOCK = state("warped_wart_block")
    private val WARPED_NYLIUM = state("warped_nylium")
    private val NETHER_WART_BLOCK = state("nether_wart_block")
    private val CRIMSON_NYLIUM = state("crimson_nylium")

    /**
     * // parity: SurfaceRuleData.overworld(HolderGetter). Always the real `overworldLike(true,
     * false, true)`. Built once and cached, not once per chunk: the tree itself carries no
     * mutable state (every node reads from the [SurfaceRuleEnvironment]/[SurfaceRules.Context]
     * passed to it at evaluation time, never its own fields), so one instance is safe to reuse,
     * including from multiple chunk-generation threads at once.
     */
    private val cachedOverworld: RuleSource by lazy {
        overworldLike(doPreliminarySurfaceCheck = true, bedrockRoof = false, bedrockFloor = true)
    }

    public fun overworld(): RuleSource = cachedOverworld

    private val cachedNether: RuleSource by lazy { buildNether() }

    /** // parity: `SurfaceRuleData.nether`. */
    public fun nether(): RuleSource = cachedNether

    // parity: SurfaceRuleData.overworldLike.
    public fun overworldLike(
        doPreliminarySurfaceCheck: Boolean,
        bedrockRoof: Boolean,
        bedrockFloor: Boolean,
    ): RuleSource {
        val woodedBadlandsTop = yBlockCheck(VerticalAnchor.absolute(97), 2)
        val badlandsTop = yBlockCheck(VerticalAnchor.absolute(256), 0)
        val badlandsHeightCondition = yStartCheck(VerticalAnchor.absolute(63), -1)
        val badlandsMid = yStartCheck(VerticalAnchor.absolute(74), 1)
        val mangroveSwampPuddleLevel = yBlockCheck(VerticalAnchor.absolute(60), 0)
        val swampPuddleLevel = yBlockCheck(VerticalAnchor.absolute(62), 0)
        val aboveOverworldSeaLevel = yBlockCheck(VerticalAnchor.absolute(63), 0)
        val notUnderwater = waterBlockCheck(-1, 0)
        val aboveWater = waterBlockCheck(0, 0)
        val notUnderDeepWater = waterStartCheck(-6, -1)
        val frozenOcean = isBiome("minecraft:frozen_ocean", "minecraft:deep_frozen_ocean")
        val steepCondition = steep()
        val grassOrDirtIfUnderwater = sequence(ifTrue(aboveWater, GRASS_BLOCK), DIRT)
        val sandOrSandstoneIfCeiling = sequence(ifTrue(SurfaceRules.ON_CEILING, SANDSTONE), SAND)
        val gravelOrStoneIfCeiling = sequence(ifTrue(SurfaceRules.ON_CEILING, STONE), GRAVEL)
        val biomesWithSandAndSandstone = isBiome("minecraft:warm_ocean", "minecraft:beach", "minecraft:snowy_beach")
        val biomesWithSandAndVeryDeepSandstone = isBiome("minecraft:desert")
        // parity: these three read the noise in 3d, unlike every other band here, and their bounds
        // are float literals in the jar, so they widen to a double a hair off the round number.
        val sulfurCaveBands =
            sequence(
                ifTrue(noiseCondition3d("sulfur_cave_gradient", -0.40000000596046448, -0.10000000149011612), CINNABAR),
                ifTrue(noiseCondition3d("sulfur_cave_gradient", 0.0, 0.40000000596046448), SULFUR),
                ifTrue(noiseCondition3d("sulfur_cave_gradient", 0.40000000596046448), CINNABAR),
            )
        val commonSurfaceAndUnderRules =
            sequence(
                ifTrue(
                    isBiome("minecraft:stony_peaks"),
                    sequence(ifTrue(noiseCondition2d("calcite", -0.0125, 0.0125), CALCITE), STONE),
                ),
                ifTrue(
                    isBiome("minecraft:stony_shore"),
                    sequence(ifTrue(noiseCondition2d("gravel", -0.05, 0.05), gravelOrStoneIfCeiling), STONE),
                ),
                ifTrue(isBiome("minecraft:windswept_hills"), ifTrue(surfaceNoiseAbove(1.0), STONE)),
                ifTrue(biomesWithSandAndSandstone, sandOrSandstoneIfCeiling),
                ifTrue(biomesWithSandAndVeryDeepSandstone, sandOrSandstoneIfCeiling),
                ifTrue(isBiome("minecraft:dripstone_caves"), STONE),
                ifTrue(isBiome("minecraft:sulfur_caves"), sequence(sulfurCaveBands, STONE)),
            )
        val powderSnowUnderRule = ifTrue(noiseCondition2d("powder_snow", 0.45, 0.58), ifTrue(aboveWater, POWDER_SNOW))
        val powderSnowSurfaceRule = ifTrue(noiseCondition2d("powder_snow", 0.35, 0.6), ifTrue(aboveWater, POWDER_SNOW))
        val biomeUnderSurfaceRule =
            sequence(
                ifTrue(
                    isBiome("minecraft:frozen_peaks"),
                    sequence(
                        ifTrue(steepCondition, PACKED_ICE),
                        ifTrue(noiseCondition2d("packed_ice", -0.5, 0.2), PACKED_ICE),
                        ifTrue(noiseCondition2d("ice", -0.0625, 0.025), ICE),
                        ifTrue(aboveWater, SNOW_BLOCK),
                    ),
                ),
                ifTrue(
                    isBiome("minecraft:snowy_slopes"),
                    sequence(ifTrue(steepCondition, STONE), powderSnowUnderRule, ifTrue(aboveWater, SNOW_BLOCK)),
                ),
                ifTrue(isBiome("minecraft:jagged_peaks"), STONE),
                ifTrue(isBiome("minecraft:grove"), sequence(powderSnowUnderRule, DIRT)),
                commonSurfaceAndUnderRules,
                ifTrue(isBiome("minecraft:windswept_savanna"), ifTrue(surfaceNoiseAbove(1.75), STONE)),
                ifTrue(
                    isBiome("minecraft:windswept_gravelly_hills"),
                    sequence(
                        ifTrue(surfaceNoiseAbove(2.0), gravelOrStoneIfCeiling),
                        ifTrue(surfaceNoiseAbove(1.0), STONE),
                        ifTrue(surfaceNoiseAbove(-1.0), DIRT),
                        gravelOrStoneIfCeiling,
                    ),
                ),
                ifTrue(isBiome("minecraft:mangrove_swamp"), MUD),
                DIRT,
            )
        val biomeSurfaceRule =
            sequence(
                ifTrue(
                    isBiome("minecraft:frozen_peaks"),
                    sequence(
                        ifTrue(steepCondition, PACKED_ICE),
                        ifTrue(noiseCondition2d("packed_ice", 0.0, 0.2), PACKED_ICE),
                        ifTrue(noiseCondition2d("ice", 0.0, 0.025), ICE),
                        ifTrue(aboveWater, SNOW_BLOCK),
                    ),
                ),
                ifTrue(
                    isBiome("minecraft:snowy_slopes"),
                    sequence(ifTrue(steepCondition, STONE), powderSnowSurfaceRule, ifTrue(aboveWater, SNOW_BLOCK)),
                ),
                ifTrue(
                    isBiome("minecraft:jagged_peaks"),
                    sequence(ifTrue(steepCondition, STONE), ifTrue(aboveWater, SNOW_BLOCK)),
                ),
                ifTrue(isBiome("minecraft:grove"), sequence(powderSnowSurfaceRule, ifTrue(aboveWater, SNOW_BLOCK))),
                commonSurfaceAndUnderRules,
                ifTrue(
                    isBiome("minecraft:windswept_savanna"),
                    sequence(ifTrue(surfaceNoiseAbove(1.75), STONE), ifTrue(surfaceNoiseAbove(-0.5), COARSE_DIRT)),
                ),
                ifTrue(
                    isBiome("minecraft:windswept_gravelly_hills"),
                    sequence(
                        ifTrue(surfaceNoiseAbove(2.0), gravelOrStoneIfCeiling),
                        ifTrue(surfaceNoiseAbove(1.0), STONE),
                        ifTrue(surfaceNoiseAbove(-1.0), grassOrDirtIfUnderwater),
                        gravelOrStoneIfCeiling,
                    ),
                ),
                ifTrue(
                    isBiome("minecraft:old_growth_pine_taiga", "minecraft:old_growth_spruce_taiga"),
                    sequence(ifTrue(surfaceNoiseAbove(1.75), COARSE_DIRT), ifTrue(surfaceNoiseAbove(-0.95), PODZOL)),
                ),
                ifTrue(isBiome("minecraft:ice_spikes"), ifTrue(aboveWater, SNOW_BLOCK)),
                ifTrue(isBiome("minecraft:mangrove_swamp"), MUD),
                ifTrue(isBiome("minecraft:mushroom_fields"), MYCELIUM),
                grassOrDirtIfUnderwater,
            )
        val clayBand1 = noiseCondition2d("surface", -0.909, -0.5454)
        val clayBand2 = noiseCondition2d("surface", -0.1818, 0.1818)
        val clayBand3 = noiseCondition2d("surface", 0.5454, 0.909)
        val mainRuleCloseToSurface =
            sequence(
                ifTrue(
                    SurfaceRules.ON_FLOOR,
                    sequence(
                        ifTrue(
                            isBiome("minecraft:wooded_badlands"),
                            ifTrue(
                                woodedBadlandsTop,
                                sequence(
                                    ifTrue(clayBand1, COARSE_DIRT),
                                    ifTrue(clayBand2, COARSE_DIRT),
                                    ifTrue(clayBand3, COARSE_DIRT),
                                    grassOrDirtIfUnderwater,
                                ),
                            ),
                        ),
                        ifTrue(
                            isBiome("minecraft:swamp"),
                            ifTrue(
                                swampPuddleLevel,
                                ifTrue(
                                    not(aboveOverworldSeaLevel),
                                    ifTrue(noiseCondition2d("surface_swamp", 0.0), WATER),
                                ),
                            ),
                        ),
                        ifTrue(
                            isBiome("minecraft:mangrove_swamp"),
                            ifTrue(
                                mangroveSwampPuddleLevel,
                                ifTrue(
                                    not(aboveOverworldSeaLevel),
                                    ifTrue(noiseCondition2d("surface_swamp", 0.0), WATER),
                                ),
                            ),
                        ),
                    ),
                ),
                ifTrue(
                    isBiome("minecraft:badlands", "minecraft:eroded_badlands", "minecraft:wooded_badlands"),
                    sequence(
                        ifTrue(
                            SurfaceRules.ON_FLOOR,
                            sequence(
                                ifTrue(badlandsTop, ORANGE_TERRACOTTA),
                                ifTrue(
                                    badlandsMid,
                                    sequence(
                                        ifTrue(clayBand1, TERRACOTTA),
                                        ifTrue(clayBand2, TERRACOTTA),
                                        ifTrue(clayBand3, TERRACOTTA),
                                        SurfaceRules.bandlands(),
                                    ),
                                ),
                                ifTrue(
                                    notUnderwater,
                                    sequence(ifTrue(SurfaceRules.ON_CEILING, RED_SANDSTONE), RED_SAND),
                                ),
                                ifTrue(not(hole()), ORANGE_TERRACOTTA),
                                ifTrue(notUnderDeepWater, WHITE_TERRACOTTA),
                                gravelOrStoneIfCeiling,
                            ),
                        ),
                        ifTrue(
                            badlandsHeightCondition,
                            sequence(
                                ifTrue(aboveOverworldSeaLevel, ifTrue(not(badlandsMid), ORANGE_TERRACOTTA)),
                                SurfaceRules.bandlands(),
                            ),
                        ),
                        ifTrue(SurfaceRules.UNDER_FLOOR, ifTrue(notUnderDeepWater, WHITE_TERRACOTTA)),
                    ),
                ),
                ifTrue(
                    SurfaceRules.ON_FLOOR,
                    ifTrue(
                        notUnderwater,
                        sequence(
                            ifTrue(
                                frozenOcean,
                                ifTrue(
                                    hole(),
                                    sequence(ifTrue(aboveWater, AIR), ifTrue(SurfaceRules.temperature(), ICE), WATER),
                                ),
                            ),
                            biomeSurfaceRule,
                        ),
                    ),
                ),
                ifTrue(
                    notUnderDeepWater,
                    sequence(
                        ifTrue(SurfaceRules.ON_FLOOR, ifTrue(frozenOcean, ifTrue(hole(), WATER))),
                        ifTrue(SurfaceRules.UNDER_FLOOR, biomeUnderSurfaceRule),
                        ifTrue(biomesWithSandAndSandstone, ifTrue(SurfaceRules.DEEP_UNDER_FLOOR, SANDSTONE)),
                        ifTrue(
                            biomesWithSandAndVeryDeepSandstone,
                            ifTrue(SurfaceRules.VERY_DEEP_UNDER_FLOOR, SANDSTONE),
                        ),
                    ),
                ),
                ifTrue(
                    SurfaceRules.ON_FLOOR,
                    sequence(
                        ifTrue(isBiome("minecraft:frozen_peaks", "minecraft:jagged_peaks"), STONE),
                        ifTrue(
                            isBiome(
                                "minecraft:warm_ocean",
                                "minecraft:lukewarm_ocean",
                                "minecraft:deep_lukewarm_ocean",
                            ),
                            sandOrSandstoneIfCeiling,
                        ),
                        gravelOrStoneIfCeiling,
                    ),
                ),
            )

        val rules = mutableListOf<RuleSource>()
        if (bedrockRoof) {
            rules +=
                ifTrue(
                    not(
                        SurfaceRules.verticalGradient("bedrock_roof", VerticalAnchor.belowTop(5), VerticalAnchor.top()),
                    ),
                    BEDROCK,
                )
        }
        if (bedrockFloor) {
            rules +=
                ifTrue(
                    SurfaceRules.verticalGradient(
                        "bedrock_floor",
                        VerticalAnchor.bottom(),
                        VerticalAnchor.aboveBottom(5),
                    ),
                    BEDROCK,
                )
        }
        val ruleAbovePreliminarySurface = ifTrue(SurfaceRules.abovePreliminarySurface(), mainRuleCloseToSurface)
        rules += if (doPreliminarySurfaceCheck) ruleAbovePreliminarySurface else mainRuleCloseToSurface
        rules += ifTrue(isBiome("minecraft:sulfur_caves"), sulfurCaveBands)
        rules +=
            ifTrue(
                SurfaceRules.verticalGradient("deepslate", VerticalAnchor.absolute(0), VerticalAnchor.absolute(8)),
                DEEPSLATE,
            )
        return sequence(*rules.toTypedArray())
    }

    private fun buildNether(): RuleSource {
        val aboveLavaLevel = yBlockCheck(VerticalAnchor.absolute(31), 0)
        val aboveLavaSurface = yBlockCheck(VerticalAnchor.absolute(32), 0)
        val aroundLavaBottom = yStartCheck(VerticalAnchor.absolute(30), 0)
        val aroundLavaTop = not(yStartCheck(VerticalAnchor.absolute(35), 0))
        val closeToCeiling = yBlockCheck(VerticalAnchor.belowTop(5), 0)
        val soulSandLayer = noiseCondition2d("soul_sand_layer", -0.012)
        val gravelLayer = noiseCondition2d("gravel_layer", -0.012)
        val patch = noiseCondition2d("patch", -0.012)
        val netherrack = noiseCondition2d("netherrack", 0.54)
        val netherWart = noiseCondition2d("nether_wart", 1.17)
        val stateSelector = noiseCondition2d("nether_state_selector", 0.0)
        val gravelPatch = ifTrue(patch, ifTrue(aroundLavaBottom, ifTrue(aroundLavaTop, GRAVEL)))

        return sequence(
            ifTrue(
                SurfaceRules.verticalGradient("bedrock_floor", VerticalAnchor.bottom(), VerticalAnchor.aboveBottom(5)),
                BEDROCK,
            ),
            ifTrue(
                not(SurfaceRules.verticalGradient("bedrock_roof", VerticalAnchor.belowTop(5), VerticalAnchor.top())),
                BEDROCK,
            ),
            ifTrue(closeToCeiling, NETHERRACK),
            ifTrue(
                isBiome("minecraft:basalt_deltas"),
                sequence(
                    ifTrue(SurfaceRules.UNDER_CEILING, BASALT),
                    ifTrue(SurfaceRules.UNDER_FLOOR, sequence(gravelPatch, ifTrue(stateSelector, BASALT), BLACKSTONE)),
                ),
            ),
            ifTrue(
                isBiome("minecraft:soul_sand_valley"),
                sequence(
                    ifTrue(
                        SurfaceRules.UNDER_CEILING,
                        sequence(ifTrue(stateSelector, SOUL_SAND), SOUL_SOIL),
                    ),
                    ifTrue(
                        SurfaceRules.UNDER_FLOOR,
                        sequence(gravelPatch, ifTrue(stateSelector, SOUL_SAND), SOUL_SOIL),
                    ),
                ),
            ),
            ifTrue(
                SurfaceRules.ON_FLOOR,
                sequence(
                    ifTrue(not(aboveLavaSurface), ifTrue(hole(), LAVA)),
                    ifTrue(
                        isBiome("minecraft:warped_forest"),
                        ifTrue(
                            not(netherrack),
                            ifTrue(
                                aboveLavaLevel,
                                sequence(ifTrue(netherWart, WARPED_WART_BLOCK), WARPED_NYLIUM),
                            ),
                        ),
                    ),
                    ifTrue(
                        isBiome("minecraft:crimson_forest"),
                        ifTrue(
                            not(netherrack),
                            ifTrue(
                                aboveLavaLevel,
                                sequence(ifTrue(netherWart, NETHER_WART_BLOCK), CRIMSON_NYLIUM),
                            ),
                        ),
                    ),
                ),
            ),
            ifTrue(
                isBiome("minecraft:nether_wastes"),
                sequence(
                    ifTrue(
                        SurfaceRules.UNDER_FLOOR,
                        ifTrue(
                            soulSandLayer,
                            sequence(
                                ifTrue(not(hole()), ifTrue(aroundLavaBottom, ifTrue(aroundLavaTop, SOUL_SAND))),
                                NETHERRACK,
                            ),
                        ),
                    ),
                    ifTrue(
                        SurfaceRules.ON_FLOOR,
                        ifTrue(
                            aboveLavaLevel,
                            ifTrue(
                                aroundLavaTop,
                                ifTrue(
                                    gravelLayer,
                                    sequence(ifTrue(aboveLavaSurface, GRAVEL), ifTrue(not(hole()), GRAVEL)),
                                ),
                            ),
                        ),
                    ),
                ),
            ),
            NETHERRACK,
        )
    }

    private fun surfaceNoiseAbove(threshold: Double): ConditionSource =
        noiseCondition2d("surface", threshold / 8.25, Double.MAX_VALUE)
}
