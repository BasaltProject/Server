package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.world.Chunk

/**
 * Real Mushroom Fields `vegetal_decoration` entries, in the real jar's own
 * `BiomeDefaultFeatures.addMushroomFieldVegetation` order (decompiled):
 * `mushroom_island_vegetation` (one position per chunk, a real `RANDOM_BOOLEAN_SELECTOR` between
 * `huge_red_mushroom` on true and `huge_brown_mushroom` on false), `brown_mushroom_taiga`
 * (rarity 4) and `red_mushroom_taiga` (rarity 256), both over the real `getMushroomPlacement`
 * modifier list.
 *
 * [ExtraVegetationDecoration.NEAR_WATER] supplies the later shared sugar-cane and firefly-bush
 * features. **Not ported**: the small mushrooms' real light-level survival rule (no light engine; the shared
 * [SimpleBlockFeature] air-and-solid-ground approximation stands in, as everywhere else here).
 */
public object MushroomFieldsDecoration : BiomeVegetalDecoration {
    override fun decorate(
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
        MushroomDecoration.TAIGA.decorate(chunk, random)
    }
}
