package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.world.Chunk

/**
 * Mushroom Fields `vegetal_decoration`, in `BiomeDefaultFeatures.addMushroomFieldVegetation` order:
 * `mushroom_island_vegetation` (one position per chunk, a `RANDOM_BOOLEAN_SELECTOR` between the
 * huge red and brown mushrooms), then `brown_mushroom_taiga` and `red_mushroom_taiga` over
 * `getMushroomPlacement`.
 *
 * [ExtraVegetationDecoration.NEAR_WATER] adds the shared sugar-cane and firefly-bush features.
 */
public object MushroomFieldsDecoration : BiomeVegetalDecoration {
    /** `mushroom_island_vegetation` without the independently seeded small mushrooms. */
    public val HUGE_ONLY: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeHugeMushroom(chunk, random)
        }

    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        placeHugeMushroom(chunk, random)
        MushroomDecoration.TAIGA.decorate(chunk, random)
    }

    private fun placeHugeMushroom(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        // parity: placed_feature/mushroom_island_vegetation.json: no count modifier, exactly one
        // scattered position per chunk, then the real random_boolean_selector draw.
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        positions = positions.flatMap { InSquarePlacement.getPositions(context, random, it) }
        positions = positions.flatMap { HeightmapPlacement.getPositions(context, random, it) }
        positions.forEach { pos ->
            val kind = if (random.nextBoolean()) HugeMushroomFeature.Kind.RED else HugeMushroomFeature.Kind.BROWN
            HugeMushroomFeature.place(chunk, random, pos.x, pos.y, pos.z, kind)
        }
    }
}
