package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.noise.SimplexNoise

/**
 * Real Plains `vegetal_decoration` entries (the real jar's own `data/minecraft/worldgen/
 * placed_feature`/`configured_feature` values for `patch_grass_plain`/`patch_tall_grass_2`/
 * `flower_plains`/`patch_bush`, verbatim), ported the same way this project already treats other
 * single named real vanilla entries (`VanillaRegistryData`'s biome/damage-type/trim-material data):
 * literal values, not a generic runtime datapack loader, since this is specific known reference
 * data, not the seed-defining terrain-shape noise `NoiseParametersJson` is scoped to.
 *
 * **Not ported yet, documented gaps:** `glow_lichen`, `brown_mushroom_normal`/`red_mushroom_normal`,
 * `patch_pumpkin`, `patch_sugar_cane`, `patch_firefly_bush_near_water` (need water/light-level
 * checks this project doesn't model yet).
 *
 * The shared [random][WorldgenRandom] this whole pipeline draws from must be the real
 * `WorldgenRandom` wrapping `XoroshiroRandomSource` (confirmed from `ChunkGenerator
 * .applyBiomeDecoration`'s own decompiled body), not the legacy LCG: an earlier version of this
 * class used a plain [LegacyRandom] here, which is a real parity bug (wrong RNG family entirely,
 * not just a different formula), caught and fixed once `WorldgenRandom`'s bridging behavior was
 * understood. [FLOWER_NOISE]/`NoiseThresholdCountPlacement`'s own `BIOME_INFO_NOISE` are the one
 * legitimate exception: real vanilla wraps a fixed-seed [LegacyRandom] there too, and since
 * `WorldgenRandom` degenerates to a pure passthrough over a bit-based inner source, using
 * [LegacyRandom] directly for those is already bit-identical.
 */
public object PlainsDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        GRASS.decorate(chunk, random)
        TALL_GRASS.decorate(chunk, random)
        placeFlowers(chunk, random)
        placeBush(chunk, random)
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

    // parity: placed_feature/patch_bush.json + configured_feature/bush.json.
    private fun placeBush(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val feature = SimpleBlockFeature({ _, _ -> bushStateId })
        runPipeline(
            chunk,
            random,
            listOf(
                RarityFilter(4),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(24),
                RandomOffsetPlacement(xzSpread = 5, ySpread = 3),
                AirTargetFilter(),
            ),
            feature,
        )
    }

    // parity: placed_feature/flower_plains.json + configured_feature/flower_plain.json.
    private fun placeFlowers(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
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
    private val bushStateId = blockId("bush")
    private val dandelionStateId = blockId("dandelion")
    private val highFlowerStates =
        listOf(blockId("poppy"), blockId("azure_bluet"), blockId("oxeye_daisy"), blockId("cornflower"))
    private val lowFlowerStates =
        listOf(blockId("orange_tulip"), blockId("red_tulip"), blockId("pink_tulip"), blockId("white_tulip"))
    private val FLOWER_NOISE = SimplexNoise(LegacyRandom(2345L))

    private fun blockId(name: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")
}
