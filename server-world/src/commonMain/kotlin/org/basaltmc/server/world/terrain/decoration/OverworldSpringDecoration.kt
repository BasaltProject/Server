package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkScheduledTick
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** // parity: the three Overworld `FLUID_SPRINGS` placed features at global indexes 0 through 2. */
public object OverworldSpringDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
    ) {
        decorateFeatures(chunk, seed, environment, 0..FROZEN_LAVA_INDEX, externalBlockState)
    }

    internal fun decorateFeatures(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        globalIndices: Iterable<Int>,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        for (globalIndex in globalIndices) {
            random.setFeatureSeed(decorationSeed, globalIndex, FLUID_SPRINGS_STEP)
            when (globalIndex) {
                WATER_INDEX ->
                    placeAttempts(
                        chunk,
                        random,
                        environment,
                        WATER,
                        25,
                        ::uniformWaterHeight,
                        externalBlockState,
                    )
                LAVA_INDEX ->
                    placeAttempts(
                        chunk,
                        random,
                        environment,
                        LAVA,
                        20,
                        ::veryBiasedHeight,
                        externalBlockState,
                    )
                FROZEN_LAVA_INDEX ->
                    placeAttempts(chunk, random, environment, FROZEN_LAVA, 20, ::veryBiasedHeight, externalBlockState)
            }
        }
    }

    private fun placeAttempts(
        chunk: Chunk,
        random: WorldgenRandom,
        environment: SurfaceRuleEnvironment,
        spring: Spring,
        count: Int,
        height: (BasaltRandom) -> Int,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
    ) {
        repeat(count) {
            val worldX = chunk.chunkX * 16 + random.nextInt(16)
            val worldZ = chunk.chunkZ * 16 + random.nextInt(16)
            val y = height(random)
            val biome = environment.biomeAt(worldX, y, worldZ).substringAfter(':')
            if (biome !in spring.biomes) return@repeat
            placeSpring(chunk, worldX, y, worldZ, spring, externalBlockState)
        }
    }

    private fun placeSpring(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        spring: Spring,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
    ) {
        fun stateAt(
            worldX: Int,
            worldY: Int,
            worldZ: Int,
        ): Int? =
            when {
                worldY !in WorldHeight.MIN_Y..WorldHeight.MAX_Y -> Blocks.AIR.stateId
                worldX shr 4 == chunk.chunkX && worldZ shr 4 == chunk.chunkZ ->
                    chunk.getBlockStateId(worldX and 15, worldY, worldZ and 15)
                else -> externalBlockState?.invoke(worldX, worldY, worldZ)
            }

        if (!spring.isValid(stateAt(x, y + 1, z))) return
        if (!spring.isValid(stateAt(x, y - 1, z))) return
        val current = stateAt(x, y, z) ?: return
        if (!Blocks.isAir(current) && !spring.isValid(current)) return

        var rockCount = 0
        var holeCount = 0
        for ((offsetX, offsetY, offsetZ) in NEIGHBOURS) {
            val neighbour = stateAt(x + offsetX, y + offsetY, z + offsetZ)
            if (spring.isValid(neighbour)) rockCount++
            if (neighbour?.let(Blocks::isAir) == true) holeCount++
        }
        if (rockCount != 4 || holeCount != 1) return

        chunk.setBlockStateId(x and 15, y, z and 15, spring.stateId)
        chunk.scheduleFluidTick(
            ChunkScheduledTick(
                spring.fluid,
                x and 15,
                y,
                z and 15,
                delay = 0,
            ),
        )
    }

    private fun uniformWaterHeight(random: BasaltRandom): Int =
        WorldHeight.MIN_Y + random.nextInt(192 - WorldHeight.MIN_Y + 1)

    private fun veryBiasedHeight(random: BasaltRandom): Int {
        val upperInclusive = nextInt(random, WorldHeight.MIN_Y + LAVA_HEIGHT_INNER, WorldHeight.MAX_Y - 8)
        val biasedUpperInclusive = nextInt(random, WorldHeight.MIN_Y, upperInclusive - 1)
        return nextInt(random, WorldHeight.MIN_Y, biasedUpperInclusive - 1 + LAVA_HEIGHT_INNER)
    }

    private fun nextInt(
        random: BasaltRandom,
        minInclusive: Int,
        maxInclusive: Int,
    ): Int {
        if (minInclusive >= maxInclusive) return minInclusive
        return minInclusive + random.nextInt(maxInclusive - minInclusive + 1)
    }

    private data class Spring(
        val stateId: Int,
        val fluid: Identifier,
        val validBlocks: Set<String>,
        val biomes: Set<String>,
    ) {
        fun isValid(stateId: Int?): Boolean = stateId?.let { Blocks.byStateId(it)?.block?.path } in validBlocks
    }

    private const val FLUID_SPRINGS_STEP = 8
    private const val WATER_INDEX = 0
    private const val LAVA_INDEX = 1
    private const val FROZEN_LAVA_INDEX = 2
    private const val LAVA_HEIGHT_INNER = 8
    private val NORMAL_VALID_BLOCKS =
        setOf("stone", "granite", "diorite", "andesite", "deepslate", "tuff", "calcite", "dirt")
    private val FROZEN_VALID_BLOCKS = setOf("snow_block", "powder_snow", "packed_ice")
    private val NEIGHBOURS =
        listOf(
            Triple(-1, 0, 0),
            Triple(1, 0, 0),
            Triple(0, 0, -1),
            Triple(0, 0, 1),
            Triple(0, -1, 0),
        )
    private val OVERWORLD_BIOMES =
        setOf(
            "plains",
            "sunflower_plains",
            "desert",
            "windswept_hills",
            "windswept_gravelly_hills",
            "windswept_forest",
            "windswept_savanna",
            "forest",
            "flower_forest",
            "birch_forest",
            "old_growth_birch_forest",
            "dark_forest",
            "old_growth_pine_taiga",
            "old_growth_spruce_taiga",
            "taiga",
            "snowy_taiga",
            "savanna",
            "savanna_plateau",
            "badlands",
            "wooded_badlands",
            "eroded_badlands",
            "jungle",
            "sparse_jungle",
            "bamboo_jungle",
            "swamp",
            "mangrove_swamp",
            "river",
            "frozen_river",
            "beach",
            "snowy_beach",
            "stony_shore",
            "snowy_plains",
            "ice_spikes",
            "ocean",
            "deep_ocean",
            "warm_ocean",
            "lukewarm_ocean",
            "deep_lukewarm_ocean",
            "cold_ocean",
            "deep_cold_ocean",
            "frozen_ocean",
            "deep_frozen_ocean",
            "mushroom_fields",
            "cherry_grove",
            "pale_garden",
            "meadow",
            "grove",
            "snowy_slopes",
            "frozen_peaks",
            "jagged_peaks",
            "stony_peaks",
            "dripstone_caves",
            "lush_caves",
            "deep_dark",
            "sulfur_caves",
        )
    private val WATER =
        Spring(
            Blocks.WATER.stateId,
            Identifier.vanilla("water"),
            NORMAL_VALID_BLOCKS + FROZEN_VALID_BLOCKS,
            OVERWORLD_BIOMES,
        )
    private val LAVA = Spring(Blocks.LAVA.stateId, Identifier.vanilla("lava"), NORMAL_VALID_BLOCKS, OVERWORLD_BIOMES)
    private val FROZEN_LAVA =
        Spring(
            Blocks.LAVA.stateId,
            Identifier.vanilla("lava"),
            FROZEN_VALID_BLOCKS,
            setOf("grove", "snowy_slopes", "frozen_peaks", "jagged_peaks"),
        )
}
