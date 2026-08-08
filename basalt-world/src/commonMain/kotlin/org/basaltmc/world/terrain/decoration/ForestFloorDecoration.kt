package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk

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
                // The registry currently stores only default states. Consume the exact weighted
                // provider choice among 4 facings x 4 flower amounts while placing that default.
                random.nextInt(16)
                wildflowersStateId
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
                // Exact provider choice is 4 facings x 3 segment amounts.
                random.nextInt(12)
                leafLitterStateId
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
            random.nextInt(16)
            wildflowersStateId
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
}
