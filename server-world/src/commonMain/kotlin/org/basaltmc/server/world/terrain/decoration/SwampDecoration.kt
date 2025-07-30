package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/**
 * Real Swamp `addSwampVegetation` ground cover beyond trees (already ported separately, see
 * `BiomeDecoration`): `flower_swamp` (blue orchid), `patch_grass_normal`, `patch_dead_bush`,
 * `patch_waterlily`, all ported from the real jar's own decompiled `VegetationPlacements`.
 * [MushroomDecoration.SWAMP] supplies the later swamp mushrooms and
 * [ExtraVegetationDecoration.SWAMP] supplies the later sugar-cane, pumpkin, and firefly-bush
 * features. Lily pads use [SimpleBlockFeature]'s exact water-or-ice survival dispatch here; the
 * production step-9 path uses the dedicated [WaterlilyPatchDecoration].
 */
public object SwampDecoration : BiomeVegetalDecoration {
    /** `patch_waterlily` only, for the exact world pipeline after dead-bush seeding. */
    public val WATERLILY: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeWaterlily(chunk, random)
        }

    /** `flower_swamp` only, after dead bushes and waterlilies moved to independent seeds. */
    public val FLOWER_ONLY: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeFlower(chunk, random)
        }

    /** `flower_swamp` and `patch_waterlily`, without the independently seeded dead-bush patch. */
    public val WITHOUT_DEAD_BUSH: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeFlower(chunk, random)
            WATERLILY.decorate(chunk, random)
        }

    /** Legacy direct component for `patch_dead_bush` plus `patch_waterlily`, without the flower. */
    public val GROUND_COVER: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeDeadBush(chunk, random)
            WATERLILY.decorate(chunk, random)
        }

    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        placeFlower(chunk, random)
        GROUND_COVER.decorate(chunk, random)
    }

    // parity: placed_feature/flower_swamp.json.

    /** The patch at one origin, which is what `/place feature` runs for `flower_swamp`. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        val feature = SimpleBlockFeature({ _, _ -> blueOrchidStateId })
        placeFrom(
            chunk,
            random,
            origin,
            listOf(CountPlacement(64), RandomOffsetPlacement(xzSpread = 6, ySpread = 2), AirTargetFilter()),
        ) { position -> feature.place(chunk, random, position) }
    }

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
    private val deadBushStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("dead_bush"))?.stateId ?: error("unknown block 'dead_bush'")
    private val lilyPadStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("lily_pad"))?.stateId ?: error("unknown block 'lily_pad'")
}
