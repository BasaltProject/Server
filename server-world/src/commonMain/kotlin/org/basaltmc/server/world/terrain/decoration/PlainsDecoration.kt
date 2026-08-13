package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.noise.SimplexNoise
import org.basaltmc.wire.common.Identifier

/**
 * The Plains `vegetal_decoration` entries, as literal values like the rest of the named vanilla
 * reference data in `VanillaRegistryData`. [BiomeDecoration] appends the shared glow lichen,
 * mushroom, pumpkin, sugar-cane and firefly-bush features; mushrooms use the surface survival
 * approximation until the light engine exists.
 *
 * The draws must come from [WorldgenRandom] over xoroshiro, not the legacy LCG: getting that wrong
 * is a whole different RNG family, and it was wrong here once. [FLOWER_NOISE] is the exception,
 * since vanilla wraps a fixed-seed [LegacyRandom] there too and the wrapper is a passthrough.
 */
public object PlainsDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        ForestGroundDecoration.BUSH.decorate(chunk, random)
        FLOWERS.decorate(chunk, random)
    }

    /** // parity: placed_feature/patch_grass_plain.json + configured_feature/grass.json, standalone: cherry_grove's own vegetation reuses exactly this real feature. */
    public val GRASS: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val feature = SimpleBlockFeature({ _, _ -> shortGrassStateId })
            runPipeline(
                chunk,
                random,
                listOf(
                    NoiseThresholdCountPlacement(-0.8, belowNoise = 10, aboveNoise = 5),
                    InSquarePlacement,
                    HeightmapPlacement,
                    CountPlacement(32),
                    RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                    AirTargetFilter(),
                ),
                feature,
            )
        }

    /** // parity: placed_feature/patch_tall_grass_2.json + configured_feature/tall_grass.json (real "addPlainGrass"), standalone: lush_caves/sulfur_caves use only this, not the rest of Plains' own decoration. */
    public val TALL_GRASS: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val feature = SimpleBlockFeature({ _, _ -> tallGrassStateId }, isDoublePlant = true)
            runPipeline(
                chunk,
                random,
                listOf(
                    NoiseThresholdCountPlacement(-0.8, belowNoise = 7, aboveNoise = 0),
                    RarityFilter(32),
                    InSquarePlacement,
                    HeightmapPlacement,
                    CountPlacement(96),
                    RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                    AirTargetFilter(alsoCheckAbove = true),
                ),
                feature,
            )
        }

    /** `patch_sunflower`: the sunflower-plains replacement for the regular plains bush. */
    public val SUNFLOWER: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val feature = SimpleBlockFeature({ _, _ -> sunflowerStateId }, isDoublePlant = true)
            runPipeline(
                chunk,
                random,
                listOf(
                    RarityFilter(3),
                    InSquarePlacement,
                    HeightmapPlacement,
                    CountPlacement(96),
                    RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                    AirTargetFilter(),
                ),
                feature,
            )
        }

    /** // parity: placed_feature/flower_plains.json + configured_feature/flower_plain.json. */
    public val FLOWERS: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val feature = SimpleBlockFeature(::flowerStateId)
            runPipeline(
                chunk,
                random,
                listOf(
                    NoiseThresholdCountPlacement(-0.8, belowNoise = 4, aboveNoise = 15),
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

    /** // parity: the feature-pool-element use of placed_feature/flower_plain at one jigsaw origin. */
    internal fun placeFlowerPlainAt(
        chunk: Chunk,
        random: WorldgenRandom,
        x: Int,
        y: Int,
        z: Int,
    ) {
        val context = PlacementContext(chunk)
        val feature = SimpleBlockFeature(::flowerStateId)
        var positions: Sequence<DecorationPos> =
            CountPlacement(
                64,
            ).getPositions(context, random, DecorationPos(x, y, z))
        positions =
            positions.flatMap { RandomOffsetPlacement(xzSpread = 6, ySpread = 2).getPositions(context, random, it) }
        positions = positions.flatMap { AirTargetFilter().getPositions(context, random, it) }
        positions.forEach { feature.place(chunk, random, it) }
    }

    /**
     * // parity: NoiseThresholdBlockStateProvider's real behavior for `flower_plain`: a dedicated
     * single-octave [SimplexNoise] (fixed legacy seed `2345`, same real construction as
     * [NoiseThresholdCountPlacement]'s own noise, real vanilla happens to reuse `2345` for both,
     * confirmed independently from each json's own real values) sampled at `x * 0.005, z * 0.005`.
     * Below the `-0.8` threshold picks a random `low_states` tulip; at or above it, a `1/3` chance
     * (real `high_chance`) of a random `high_states` flower, otherwise the default (dandelion).
     * Which specific flower within a bucket is chosen uses the pipeline's own shared random (the
     * same source vanilla's real `getOptionalState(level, random, pos)` draws from), not a
     * position hash.
     */
    private fun flowerStateId(
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Int {
        val noiseValue = FLOWER_NOISE.getValue(origin.x * 0.005, origin.z * 0.005)
        return if (noiseValue < -0.8) {
            lowFlowerStates[random.nextInt(lowFlowerStates.size)]
        } else if (random.nextFloat() < 1.0f / 3.0f) {
            highFlowerStates[random.nextInt(highFlowerStates.size)]
        } else {
            dandelionStateId
        }
    }

    private fun runPipeline(
        chunk: Chunk,
        random: WorldgenRandom,
        modifiers: List<PlacementModifier>,
        feature: SimpleBlockFeature,
    ) {
        // parity: PlacedFeature.placeWithContext starts from a single origin (the chunk's own
        // corner) and lets the modifier pipeline itself do all the scattering (`count`/
        // `noise_threshold_count` duplicate, `in_square` picks a new random column each time):
        // running the whole pipeline once per chunk, not once per column.
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in modifiers) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { pos -> feature.place(chunk, random, pos) }
    }

    private val shortGrassStateId = blockId("short_grass")
    private val tallGrassStateId = blockId("tall_grass")
    private val sunflowerStateId = blockId("sunflower")
    private val dandelionStateId = blockId("dandelion")
    private val highFlowerStates =
        listOf(blockId("poppy"), blockId("azure_bluet"), blockId("oxeye_daisy"), blockId("cornflower"))
    private val lowFlowerStates =
        listOf(blockId("orange_tulip"), blockId("red_tulip"), blockId("pink_tulip"), blockId("white_tulip"))
    private val FLOWER_NOISE = SimplexNoise(LegacyRandom(2345L))

    private fun blockId(name: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")
}
