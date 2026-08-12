package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/** Shared small-mushroom placed features used by Overworld biomes. */
public object MushroomDecoration {
    /** `addDefaultMushrooms`: brown 1/256, then red 1/512. */
    public val NORMAL: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placePatch(chunk, random, brownMushroomStateId, rarity = 256)
            placePatch(chunk, random, redMushroomStateId, rarity = 512)
        }

    /** The mushroom tail of `addTaigaGrass`: brown 1/4, then red 1/256. */
    public val TAIGA: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placePatch(chunk, random, brownMushroomStateId, rarity = 4)
            placePatch(chunk, random, redMushroomStateId, rarity = 256)
        }

    /** Old-growth mushrooms, followed separately by [NORMAL] in those biomes. */
    public val OLD_GROWTH: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placePatch(chunk, random, brownMushroomStateId, rarity = 4, outerCount = 3)
            placePatch(chunk, random, redMushroomStateId, rarity = 171)
        }

    /** Swamp mushrooms, followed separately by [NORMAL]. */
    public val SWAMP: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placePatch(chunk, random, brownMushroomStateId, rarity = null, outerCount = 2)
            placePatch(chunk, random, redMushroomStateId, rarity = 64)
        }

    private fun placePatch(
        chunk: Chunk,
        random: WorldgenRandom,
        stateId: Int,
        rarity: Int?,
        outerCount: Int = 1,
    ) {
        val feature = SimpleBlockFeature({ _, _ -> stateId })
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        if (outerCount !=
            1
        ) {
            positions = positions.flatMap { CountPlacement(outerCount).getPositions(context, random, it) }
        }
        if (rarity != null) positions = positions.flatMap { RarityFilter(rarity).getPositions(context, random, it) }
        for (
        modifier in
        listOf(
            InSquarePlacement,
            HeightmapPlacement,
            CountPlacement(96),
            RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
            AirTargetFilter(),
        )
        ) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { position -> feature.place(chunk, random, position) }
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val brownMushroomStateId = stateId("brown_mushroom")
    private val redMushroomStateId = stateId("red_mushroom")
}
