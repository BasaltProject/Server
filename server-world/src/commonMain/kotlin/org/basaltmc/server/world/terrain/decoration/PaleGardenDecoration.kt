package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/**
 * Closed-eyeblossom placed features used only by the pale garden. The configured features request
 * `schedule_tick=true`; placement is present now, while scheduling awaits the block-tick engine.
 */
public object PaleGardenDecoration {
    private val closedEyeblossomStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("closed_eyeblossom"))?.stateId
            ?: error("unknown block 'closed_eyeblossom'")

    /** `pale_garden_flowers`: rarity 1/8, then a 96-candidate patch below leaf cover. */
    public val FLOWER_PATCH: BiomeVegetalDecoration =
        placed(
            listOf(
                RarityFilter(8),
                InSquarePlacement,
                HeightmapPlacement(Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES),
                CountPlacement(96),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                AirTargetFilter(),
            ),
        )

    /** `flower_pale_garden`: a separate rarity-1/32 single placement. */
    public val RARE_FLOWER: BiomeVegetalDecoration =
        placed(
            listOf(
                RarityFilter(32),
                InSquarePlacement,
                HeightmapPlacement,
            ),
        )

    /** The patch at one origin, which is what `/place feature` runs for `flower_pale_garden`. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        val feature = SimpleBlockFeature({ _, _ -> closedEyeblossomStateId })
        placeFrom(
            chunk,
            random,
            origin,
            listOf(CountPlacement(96), RandomOffsetPlacement(xzSpread = 7, ySpread = 3), AirTargetFilter()),
        ) { position -> feature.place(chunk, random, position) }
    }

    private fun placed(modifiers: List<PlacementModifier>): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val context = PlacementContext(chunk)
            val feature = SimpleBlockFeature({ _, _ -> closedEyeblossomStateId })
            var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
            for (modifier in modifiers) positions = positions.flatMap { modifier.getPositions(context, random, it) }
            positions.forEach { feature.place(chunk, random, it) }
        }
}
