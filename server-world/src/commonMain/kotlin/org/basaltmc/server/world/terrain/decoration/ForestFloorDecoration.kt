package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/** Specialized birch and dark-forest floor plants. */
public object ForestFloorDecoration {
    private val wildflowersStateId = stateId("wildflowers")
    private val leafLitterStateId = stateId("leaf_litter")

    /** `wildflowers_birch_forest`: three independent rarity-1/2 patch attempts. */
    public val BIRCH_WILDFLOWERS: BiomeVegetalDecoration =
        placed(
            listOf(
                CountPlacement(3),
                RarityFilter(2),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(64),
                RandomOffsetPlacement(xzSpread = 6, ySpread = 2),
                AirTargetFilter(),
            ),
            SimpleBlockFeature({ random, _ ->
                flowerBedStateId(wildflowersStateId, random.nextInt(16))
            }),
        )

    /** `wildflowers_meadow`: biome-noise outer density and eight candidates per patch. */
    public val MEADOW_WILDFLOWERS: BiomeVegetalDecoration =
        placed(
            listOf(
                NoiseThresholdCountPlacement(-0.8, belowNoise = 5, aboveNoise = 10),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(8),
                RandomOffsetPlacement(xzSpread = 6, ySpread = 2),
                AirTargetFilter(),
            ),
            wildflowerFeature(),
        )

    internal fun placeWildflowersAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        count: Int,
    ) {
        val feature = wildflowerFeature()
        placeFrom(
            chunk,
            random,
            origin,
            listOf(CountPlacement(count), RandomOffsetPlacement(xzSpread = 6, ySpread = 2), AirTargetFilter()),
        ) { position -> feature.place(chunk, random, position) }
    }

    /** `patch_leaf_litter`: two 32-candidate patches restricted to grass block. */
    public val LEAF_LITTER: BiomeVegetalDecoration =
        placed(
            listOf(
                CountPlacement(2),
                InSquarePlacement,
                HeightmapPlacement(Chunk.HeightmapType.WORLD_SURFACE),
                CountPlacement(32),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                AirTargetFilter(),
                BlockBelowFilter(Blocks.GRASS_BLOCK.stateId),
            ),
            SimpleBlockFeature({ random, _ ->
                leafLitterStateId(random.nextInt(12), 3)
            }),
        )

    private fun placed(
        modifiers: List<PlacementModifier>,
        feature: SimpleBlockFeature,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val context = PlacementContext(chunk)
            var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
            for (modifier in modifiers) positions = positions.flatMap { modifier.getPositions(context, random, it) }
            positions.forEach { feature.place(chunk, random, it) }
        }

    private fun wildflowerFeature(): SimpleBlockFeature =
        SimpleBlockFeature({ random, _ ->
            flowerBedStateId(wildflowersStateId, random.nextInt(16))
        })

    private class BlockBelowFilter(
        private val stateId: Int,
    ) : PlacementFilter {
        override fun shouldPlace(
            context: PlacementContext,
            random: WorldgenRandom,
            origin: DecorationPos,
        ): Boolean = context.chunk.getBlockStateId(origin.x and 15, origin.y - 1, origin.z and 15) == stateId
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private fun leafLitterStateId(
        choice: Int,
        maxSegmentAmount: Int,
    ): Int {
        val segmentOffset = choice / 4
        val facingOffset = HORIZONTAL_FACING_STATE_OFFSETS[choice % 4]
        check(segmentOffset < maxSegmentAmount)
        return leafLitterStateId + facingOffset + segmentOffset
    }

    private fun flowerBedStateId(
        defaultStateId: Int,
        choice: Int,
    ): Int {
        val amountOffset = choice / 4
        val facingOffset = HORIZONTAL_FACING_STATE_OFFSETS[choice % 4]
        return defaultStateId + facingOffset + amountOffset
    }

    // Direction.Plane.HORIZONTAL provider order is NORTH, EAST, SOUTH, WEST, while block-state
    // IDs group facings as NORTH, SOUTH, WEST, EAST.
    private val HORIZONTAL_FACING_STATE_OFFSETS = intArrayOf(0, 12, 4, 8)

    internal fun flowerBedStateIdForTest(defaultStateId: Int, choice: Int): Int =
        flowerBedStateId(defaultStateId, choice)

}
