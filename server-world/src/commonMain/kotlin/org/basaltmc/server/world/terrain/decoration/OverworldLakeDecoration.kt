package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/**
 * The four Overworld LAKES nodes in FeatureSorter index order. Every biome but `deep_dark` lists the
 * two lava lakes first, so the global order is forced and needs no tie-breaking.
 */
public object OverworldLakeDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
    ) {
        for (spec in SPECS) decorateFeature(chunk, seed, environment, spec)
    }

    internal fun decorateFeature(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        name: String,
    ) {
        decorateFeature(chunk, seed, environment, SPECS.single { it.name == name })
    }

    private fun decorateFeature(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        spec: LakeSpec,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, spec.index, LAKES_STEP)
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> =
            sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in spec.modifiers) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { position ->
            val biome = environment.biomeAt(position.x, position.y, position.z).substringAfter(':')
            if (biome !in spec.biomes) return@forEach
            StandaloneFeatures.placeAt(chunk, random, position, spec.name)
        }
    }

    private class LakeSpec(
        val name: String,
        val index: Int,
        val modifiers: List<PlacementModifier>,
        val biomes: Set<String>,
    )

    private fun stateAt(
        chunk: Chunk,
        position: DecorationPos,
    ): Int = chunk.getDecorationBlockStateId(position.x, position.y, position.z)

    private val SULFUR_STATE_ID =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("sulfur"))?.stateId ?: error("unknown block 'sulfur'")

    /** Every Overworld biome carries the two lava lakes except `deep_dark`, which lists none. */
    private val LAVA_LAKE_BIOMES: Set<String> = ALL_BIOMES - "deep_dark"
    private val SULFUR_CAVES = setOf("sulfur_caves")

    private val SPECS =
        listOf(
            LakeSpec(
                "lake_lava_underground",
                0,
                listOf(
                    RarityFilter(9),
                    InSquarePlacement,
                    UniformHeightRangePlacement(0, BELOW_TOP),
                    EnvironmentScanPlacement(
                        directionY = -1,
                        maxSteps = 32,
                        target = { stateId, position ->
                            !Blocks.isAir(stateId) && position.y - 5 >= WorldHeight.MIN_Y
                        },
                    ),
                    SurfaceRelativeThresholdFilter(Chunk.HeightmapType.OCEAN_FLOOR, maxInclusive = -5),
                ),
                LAVA_LAKE_BIOMES,
            ),
            LakeSpec(
                "lake_lava_surface",
                1,
                listOf(
                    RarityFilter(200),
                    InSquarePlacement,
                    HeightmapPlacement(Chunk.HeightmapType.WORLD_SURFACE),
                ),
                LAVA_LAKE_BIOMES,
            ),
            LakeSpec(
                "rooted_sulfur_spring",
                2,
                listOf(
                    CountPlacement(IntProvider.Uniform(1, 2)),
                    InSquarePlacement,
                    UniformHeightRangePlacement(WorldHeight.MIN_Y, 256),
                    EnvironmentScanPlacement(
                        directionY = 1,
                        maxSteps = 12,
                        target = { stateId, _ -> BlockShapes.isSolidState(stateId) },
                        allowed = { stateId, _ -> Blocks.isAir(stateId) },
                    ),
                    ConstantOffsetPlacement(xzOffset = 0, yOffset = -1),
                ),
                SULFUR_CAVES,
            ),
            LakeSpec(
                "sulfur_pool",
                3,
                listOf(
                    CountPlacement(256),
                    InSquarePlacement,
                    UniformHeightRangePlacement(WorldHeight.MIN_Y, 256),
                    PlacementFilter { context, _, origin -> BlockShapes.isSolidState(stateAt(context.chunk, origin)) },
                    EnvironmentScanPlacement(
                        directionY = 1,
                        maxSteps = 32,
                        target = { stateId, _ -> Blocks.isAir(stateId) },
                    ),
                    ConstantOffsetPlacement(xzOffset = 0, yOffset = -1),
                    PlacementFilter { context, _, origin -> stateAt(context.chunk, origin) == SULFUR_STATE_ID },
                ),
                SULFUR_CAVES,
            ),
        ).also { specs -> check(specs.map(LakeSpec::index) == (0..3).toList()) }

    private const val LAKES_STEP = 1

    /** `VerticalAnchor.belowTop(0)`, the top block the world can generate. */
    private const val BELOW_TOP = 319
}
