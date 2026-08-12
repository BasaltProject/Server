package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/** Savanna tall-grass pre-pass and post-tree grass patches. */
public object SavannaGroundDecoration {
    /** `patch_tall_grass`, used before trees by non-windswept savannas. */
    public val TALL_GRASS: BiomeVegetalDecoration =
        placed(
            listOf(
                RarityFilter(5),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(96),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                AirTargetFilter(alsoCheckAbove = true),
            ),
            SimpleBlockFeature({ _, _ -> tallGrassStateId }, isDoublePlant = true),
        )

    /** `patch_grass_savanna`, twenty outer attempts after trees and warm flowers. */
    public val SAVANNA: BiomeVegetalDecoration = grass(outerCount = 20)

    /** `patch_grass_normal`, five outer attempts in windswept savanna. */
    public val WINDSWEPT: BiomeVegetalDecoration = grass(outerCount = 5)

    private fun grass(outerCount: Int): BiomeVegetalDecoration =
        placed(
            listOf(
                CountPlacement(outerCount),
                InSquarePlacement,
                HeightmapPlacement(Chunk.HeightmapType.WORLD_SURFACE),
                CountPlacement(32),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                AirTargetFilter(),
            ),
            SimpleBlockFeature({ _, _ -> shortGrassStateId }),
        )

    private fun placed(
        modifiers: List<PlacementModifier>,
        feature: SimpleBlockFeature,
    ): BiomeVegetalDecoration = BiomeVegetalDecoration { chunk, random -> place(chunk, random, modifiers, feature) }

    private fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        modifiers: List<PlacementModifier>,
        feature: SimpleBlockFeature,
    ) {
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in modifiers) positions = positions.flatMap { modifier.getPositions(context, random, it) }
        positions.forEach { position -> feature.place(chunk, random, position) }
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val shortGrassStateId = stateId("short_grass")
    private val tallGrassStateId = stateId("tall_grass")
}
