package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/** Shared forest, mountain, river, and plains bush/grass placed features. */
public object ForestGroundDecoration {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val bushStateId = stateId("bush")
    private val shortGrassStateId = stateId("short_grass")

    /** `patch_bush`: rarity 1/4, then 24 candidates with 5/3 triangle offsets. */
    public val BUSH: BiomeVegetalDecoration =
        placed(
            listOf(
                RarityFilter(4),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(24),
                RandomOffsetPlacement(xzSpread = 5, ySpread = 3),
                AirTargetFilter(),
            ),
            bushStateId,
        )

    /** `patch_grass_forest`: two outer attempts, then 32 candidates each. */
    public val GRASS: BiomeVegetalDecoration =
        placed(
            listOf(
                CountPlacement(2),
                InSquarePlacement,
                HeightmapPlacement(Chunk.HeightmapType.WORLD_SURFACE),
                CountPlacement(32),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                AirTargetFilter(),
            ),
            shortGrassStateId,
        )

    private fun placed(
        modifiers: List<PlacementModifier>,
        stateId: Int,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val context = PlacementContext(chunk)
            val feature = SimpleBlockFeature({ _, _ -> stateId })
            var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
            for (modifier in modifiers) positions = positions.flatMap { modifier.getPositions(context, random, it) }
            positions.forEach { position -> feature.place(chunk, random, position) }
        }
}
