package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/**
 * The real shared "default" grass and flower patches most Overworld biomes fall back to
 * (`BiomeDefaultFeatures.addDefaultGrass`/`addDefaultFlowers`, `VegetationPlacements
 * .PATCH_GRASS_BADLANDS`/`FLOWER_DEFAULT`, decompiled), used by many biomes beyond just badlands
 * (windswept hills, taiga, river, beach, frozen ocean...) despite the misleading real name. Flower
 * choice is `WeightedStateProvider` real weights `poppy:2, dandelion:1`: `nextInt(3) < 2` picks
 * poppy, matching this project's own established cumulative-weight reduction (see
 * `CountExtraPlacement`'s own KDoc for the same reasoning), not a `NoiseProvider`/noise threshold
 * like Plains' own flower selection.
 */
public object DefaultVegetationDecoration : BiomeVegetalDecoration {
    /** // parity: placed_feature/patch_grass_badlands.json (real "default grass"), standalone: some biomes (badlands) use only this, not the flowers. */
    public val GRASS: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val feature = SimpleBlockFeature({ _, _ -> shortGrassStateId })
            runPipeline(
                chunk,
                random,
                listOf(
                    InSquarePlacement,
                    HeightmapPlacement,
                    CountPlacement(32),
                    RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                    AirTargetFilter(),
                ),
            ) { c, r, pos -> feature.place(c, r, pos) }
        }

    /** // parity: placed_feature/flower_default.json, standalone: some biomes (taiga) use only this, not the grass. */
    public val FLOWERS: BiomeVegetalDecoration = flowers(rarity = 32)

    /** // parity: placed_feature/flower_warm.json: the exact same real `flowerDefault` configured feature as [FLOWERS], just twice as common (jungle/savanna, non-shattered). */
    public val WARM_FLOWERS: BiomeVegetalDecoration = flowers(rarity = 16)

    /** The vanilla placed feature names of the flower patches this draws. */
    public val PLACED_IDS: Set<String> = setOf("flower_default", "flower_warm")

    /** The patch at one origin, which is what `/place feature` runs. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        val feature = SimpleBlockFeature({ r, _ -> if (r.nextInt(3) < 2) poppyStateId else dandelionStateId })
        placeFrom(
            chunk,
            random,
            origin,
            listOf(CountPlacement(64), RandomOffsetPlacement(xzSpread = 6, ySpread = 2), AirTargetFilter()),
        ) { position -> feature.place(chunk, random, position) }
    }

    private fun flowers(rarity: Int): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val feature = SimpleBlockFeature({ r, _ -> if (r.nextInt(3) < 2) poppyStateId else dandelionStateId })
            runPipeline(
                chunk,
                random,
                listOf(
                    RarityFilter(rarity),
                    InSquarePlacement,
                    HeightmapPlacement,
                    CountPlacement(64),
                    RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                    AirTargetFilter(),
                ),
            ) { c, r, pos -> feature.place(c, r, pos) }
        }

    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        FLOWERS.decorate(chunk, random)
    }

    private fun runPipeline(
        chunk: Chunk,
        random: WorldgenRandom,
        modifiers: List<PlacementModifier>,
        place: (Chunk, WorldgenRandom, DecorationPos) -> Boolean,
    ) {
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in modifiers) positions = positions.flatMap { modifier.getPositions(context, random, it) }
        positions.forEach { pos -> place(chunk, random, pos) }
    }

    private val shortGrassStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("short_grass"))?.stateId ?: error("unknown block 'short_grass'")
    private val poppyStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("poppy"))?.stateId ?: error("unknown block 'poppy'")
    private val dandelionStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("dandelion"))?.stateId ?: error("unknown block 'dandelion'")
}
