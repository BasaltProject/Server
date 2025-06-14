package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/** The two `forest_flowers` simple-random-selector placed-feature variants. */
public object ForestFlowerDecoration {
    /** Standard forests: rarity 1/7 and clamped uniform(-3..1, 0..1) outer count. */
    public val NORMAL: BiomeVegetalDecoration =
        variant(IntProvider.Clamped(IntProvider.Uniform(-3, 1), min = 0, max = 1))

    /** Flower forest: rarity 1/7 and clamped uniform(-1..3, 0..3) outer count. */
    public val FLOWER_FOREST: BiomeVegetalDecoration =
        variant(IntProvider.Clamped(IntProvider.Uniform(-1, 3), min = 0, max = 3))

    private fun variant(outerCount: IntProvider): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val context = PlacementContext(chunk)
            var positions: Sequence<DecorationPos> = sequenceOf(chunkOrigin(chunk))
            for (
            modifier in
            listOf(
                RarityFilter(7),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(outerCount),
            )
            ) {
                positions = positions.flatMap { modifier.getPositions(context, random, it) }
            }
            positions.forEach { origin -> placeSelectedPatch(chunk, context, random, origin) }
        }

    /** The vanilla placed feature names this step draws. */
    public val PLACED_IDS: Set<String> = setOf("forest_flowers", "flower_flower_forest", "flower_forest_flowers")

    /** The flower patch at one origin, which is what `/place feature` runs. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        placeSelectedPatch(chunk, PlacementContext(chunk), random, origin)
    }

    private fun placeSelectedPatch(
        chunk: Chunk,
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        val selected = flowerStates[random.nextInt(flowerStates.size)]
        val feature = SimpleBlockFeature({ _, _ -> selected.stateId }, isDoublePlant = selected.doublePlant)
        var positions: Sequence<DecorationPos> = sequenceOf(origin)
        for (
        modifier in
        listOf(
            CountPlacement(96),
            RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
            AirTargetFilter(),
        )
        ) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { feature.place(chunk, random, it) }
    }

    private fun chunkOrigin(chunk: Chunk): DecorationPos = DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16)

    private data class FlowerState(
        val stateId: Int,
        val doublePlant: Boolean,
    )

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    // parity: SimpleRandomSelectorFeature preserves this configured-feature insertion order.
    private val flowerStates =
        listOf(
            FlowerState(stateId("lilac"), true),
            FlowerState(stateId("rose_bush"), true),
            FlowerState(stateId("peony"), true),
            FlowerState(stateId("lily_of_the_valley"), false),
        )
}
