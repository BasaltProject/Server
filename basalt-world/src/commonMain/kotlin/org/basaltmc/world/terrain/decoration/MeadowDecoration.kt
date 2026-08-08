package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk

/**
 * Real Meadow `patch_grass_meadow`, ported from the real jar's own `VegetationPlacements`.
 * **Not ported**: `flower_meadow` (a `DualNoiseProvider` weighted flower mix across two noise
 * dimensions, a more elaborate block-state provider than this project models yet),
 * `wildflowers_meadow`, `trees_meadow` (rare, `RarityFilter(100)`, a `RANDOM_SELECTOR` between an
 * unported `fancy_oak_bees` and `super_birch_bees`, which this project's [TreeDecoration] can
 * express but wasn't prioritized given the real feature is already rare 1/100 per chunk).
 */
public object MeadowDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val feature = SimpleBlockFeature({ _, _ -> shortGrassStateId })
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        // parity: placed_feature/patch_grass_meadow.json.
        for (modifier in
            listOf(
                NoiseThresholdCountPlacement(-0.8, belowNoise = 5, aboveNoise = 10),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(16),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                AirTargetFilter(),
            )
        ) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { pos -> feature.place(chunk, random, pos) }
    }

    private val shortGrassStateId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("short_grass"))?.stateId ?: error("unknown block 'short_grass'")
}
