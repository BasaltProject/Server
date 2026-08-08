package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk

/**
 * Real Mushroom Fields `vegetal_decoration` entries, in the real jar's own
 * `BiomeDefaultFeatures.addMushroomFieldVegetation` order (decompiled):
 * `mushroom_island_vegetation` (one position per chunk, a real `RANDOM_BOOLEAN_SELECTOR` between
 * `huge_red_mushroom` on true and `huge_brown_mushroom` on false), `brown_mushroom_taiga`
 * (rarity 4) and `red_mushroom_taiga` (rarity 256), both over the real `getMushroomPlacement`
 * modifier list.
 *
 * **Not ported**: `addNearWaterVegetation` (`patch_sugar_cane`, needs water modeling) and the
 * small mushrooms' real light-level survival rule (no light engine; the shared
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
        placeSmallMushrooms(chunk, random, 4, brownMushroomStateId)
        placeSmallMushrooms(chunk, random, 256, redMushroomStateId)
    }

    // parity: placed_feature/brown_mushroom_taiga.json + red_mushroom_taiga.json (the real
    // getMushroomPlacement list) over configured_feature/brown_mushroom.json / red_mushroom.json.
    private fun placeSmallMushrooms(
        chunk: Chunk,
        random: WorldgenRandom,
        rarity: Int,
        stateId: Int,
    ) {
        val feature = SimpleBlockFeature({ _, _ -> stateId })
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in
            listOf(
                RarityFilter(rarity),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(96),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                AirTargetFilter(),
            )
        ) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { pos -> feature.place(chunk, random, pos) }
    }

    private val brownMushroomStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("brown_mushroom"))?.stateId ?: error("unknown block 'brown_mushroom'")
    private val redMushroomStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("red_mushroom"))?.stateId ?: error("unknown block 'red_mushroom'")
}
