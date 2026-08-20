package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.decoration.tree.TreeConfigs
import org.basaltmc.wire.common.Identifier

/**
 * Real Cherry Grove `vegetal_decoration` entries, in the real jar's own
 * `BiomeDefaultFeatures.addCherryGroveVegetation` order (decompiled). [GrassPatchDecoration]
 * supplies the preceding `patch_grass_plain`; this component retains `flower_cherry` and
 * `trees_cherry`.
 *
 * Pink petals preserve all 16 weighted block states: amount 1-4 and four horizontal facings.
 */
public object CherryGroveDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        placePinkPetals(chunk, random)
        TreeDecoration.single(TreeConfigs.CHERRY_BEES_005, 10, 0.1f, 1).decorate(chunk, random)
    }

    /** Petal-only tail used after [TreePatchDecoration] owns `trees_cherry`. */
    public val FLOWERS_ONLY: BiomeVegetalDecoration = BiomeVegetalDecoration(::placePinkPetals)

    // parity: placed_feature/flower_cherry.json + configured_feature/flower_cherry.json
    // (SIMPLE_BLOCK over a WeightedStateProvider of the 16 pink_petals states).

    /** The petal patch at one origin, which is what `/place feature` runs for `flower_cherry`. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        val feature = SimpleBlockFeature({ r, _ -> pinkPetalsStateId(r.nextInt(16)) })
        placeFrom(
            chunk,
            random,
            origin,
            listOf(CountPlacement(96), RandomOffsetPlacement(xzSpread = 6, ySpread = 2), AirTargetFilter()),
        ) { position -> feature.place(chunk, random, position) }
    }

    private fun placePinkPetals(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val feature =
            SimpleBlockFeature({ r, _ ->
                pinkPetalsStateId(r.nextInt(16))
            })
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (
        modifier in
        listOf(
            NoiseThresholdCountPlacement(-0.8, belowNoise = 5, aboveNoise = 10),
            InSquarePlacement,
            HeightmapPlacement,
            CountPlacement(96),
            RandomOffsetPlacement(xzSpread = 6, ySpread = 2),
            AirTargetFilter(),
        )
        ) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { pos -> feature.place(chunk, random, pos) }
    }

    private val pinkPetalsStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("pink_petals"))?.stateId ?: error("unknown block 'pink_petals'")

    private fun pinkPetalsStateId(choice: Int): Int {
        val amountOffset = choice / 4
        val facingOffset = HORIZONTAL_FACING_STATE_OFFSETS[choice % 4]
        return pinkPetalsStateId + facingOffset + amountOffset
    }

    private val HORIZONTAL_FACING_STATE_OFFSETS = intArrayOf(0, 12, 4, 8)
}
