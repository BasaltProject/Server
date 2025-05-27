package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk

/**
 * Real Swamp `addSwampVegetation` ground cover beyond trees (already ported separately, see
 * `BiomeDecoration`): `flower_swamp` (blue orchid), `patch_grass_normal`, `patch_dead_bush`,
 * `patch_waterlily`, all ported from the real jar's own decompiled `VegetationPlacements`.
 * [MushroomDecoration.SWAMP] supplies the later swamp mushrooms, using the shared surface survival
 * approximation until a light engine exists. [ExtraVegetationDecoration.SWAMP] supplies the later
 * sugar-cane, pumpkin, and firefly-bush features. Lily pads are placed the same simplified
 * way as every other single-block feature here (air target only, no "must be over water" check,
 * see [SimpleBlockFeature]'s own KDoc).
 */
public object SwampDecoration : BiomeVegetalDecoration {
    /** `patch_grass_normal`+`patch_dead_bush`+`patch_waterlily`, without the blue-orchid flower: real `mangrove_swamp` shares exactly this ground cover, not the flower. */
    public val GROUND_COVER: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeGrass(chunk, random)
            placeDeadBush(chunk, random)
            placeWaterlily(chunk, random)
        }

    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        placeFlower(chunk, random)
        GROUND_COVER.decorate(chunk, random)
    }

    // parity: placed_feature/flower_swamp.json.
    private fun placeFlower(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val feature = SimpleBlockFeature({ _, _ -> blueOrchidStateId })
        runPipeline(
            chunk,
            random,
            listOf(
                RarityFilter(32),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(64),
                RandomOffsetPlacement(xzSpread = 6, ySpread = 2),
                AirTargetFilter(),
            ),
            feature,
        )
    }

    // parity: placed_feature/patch_grass_normal.json.
    private fun placeGrass(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val feature = SimpleBlockFeature({ _, _ -> shortGrassStateId })
        runPipeline(
            chunk,
            random,
            listOf(
                CountPlacement(5),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(32),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                AirTargetFilter(),
            ),
            feature,
        )
    }

    // parity: placed_feature/patch_dead_bush.json.
    private fun placeDeadBush(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val feature = SimpleBlockFeature({ _, _ -> deadBushStateId })
        runPipeline(
            chunk,
            random,
            listOf(
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(4),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                AirTargetFilter(),
            ),
            feature,
        )
    }

    // parity: placed_feature/patch_waterlily.json.
    private fun placeWaterlily(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val feature = SimpleBlockFeature({ _, _ -> lilyPadStateId })
        runPipeline(
            chunk,
            random,
            listOf(
                CountPlacement(4),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(10),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                AirTargetFilter(),
            ),
            feature,
        )
    }

    private fun runPipeline(
        chunk: Chunk,
        random: WorldgenRandom,
        modifiers: List<PlacementModifier>,
        feature: SimpleBlockFeature,
    ) {
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in modifiers) positions = positions.flatMap { modifier.getPositions(context, random, it) }
        positions.forEach { pos -> feature.place(chunk, random, pos) }
    }

    private val blueOrchidStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("blue_orchid"))?.stateId ?: error("unknown block 'blue_orchid'")
    private val shortGrassStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("short_grass"))?.stateId ?: error("unknown block 'short_grass'")
    private val deadBushStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("dead_bush"))?.stateId ?: error("unknown block 'dead_bush'")
    private val lilyPadStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("lily_pad"))?.stateId ?: error("unknown block 'lily_pad'")
}
