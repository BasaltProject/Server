package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.terrain.decoration.tree.TreeConfigs

/**
 * Real Cherry Grove `vegetal_decoration` entries, in the real jar's own
 * `BiomeDefaultFeatures.addCherryGroveVegetation` order (decompiled): `patch_grass_plain` (the
 * exact real feature [PlainsDecoration.GRASS] builds), `flower_cherry` (pink petals) and
 * `trees_cherry`.
 *
 * **Documented gaps**: real `trees_cherry` points at `cherry_bees_005`, the plain cherry config
 * plus a `BeehiveDecorator(0.05)`; tree decorators aren't ported (see TreeFeature.kt's own KDoc).
 * Pink petals' 16 real weighted block states (amount 1-4 x 4 horizontal facings, one weight each)
 * collapse to the default state here (block states aren't modeled yet, the same documented gap as
 * leaf `distance`); the provider's real `nextInt(16)` draw is still consumed per placed position
 * to keep the shared random stream aligned with real vanilla.
 */
public object CherryGroveDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        PlainsDecoration.GRASS.decorate(chunk, random)
        placePinkPetals(chunk, random)
        TreeDecoration.single(TreeConfigs.CHERRY, 10, 0.1f, 1).decorate(chunk, random)
    }

    // parity: placed_feature/flower_cherry.json + configured_feature/flower_cherry.json
    // (SIMPLE_BLOCK over a WeightedStateProvider of the 16 pink_petals states).
    private fun placePinkPetals(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val feature =
            SimpleBlockFeature({ r, _ ->
                r.nextInt(16)
                pinkPetalsStateId
            })
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in
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
}
