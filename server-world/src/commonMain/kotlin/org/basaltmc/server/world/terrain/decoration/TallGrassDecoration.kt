package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** // parity: the patch_tall_grass and patch_tall_grass_2 placed features. */
public object TallGrassDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        decorateFeature(SAVANNA_INDEX, chunk, seed, environment, externalBlockState, blockWriteObserver)
        decorateFeature(PLAIN_INDEX, chunk, seed, environment, externalBlockState, blockWriteObserver)
    }

    internal fun decorateFeature(
        index: Int,
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        val region = TallGrassRegion(chunk, externalBlockState, blockWriteObserver)
        val context = PlacementContext(chunk)
        random.setFeatureSeed(decorationSeed, index, VEGETAL_DECORATION_STEP)
        when (index) {
            SAVANNA_INDEX -> placeSavanna(context, region, random, environment)
            PLAIN_INDEX -> placePlain(context, region, random, environment)
            else -> error("unknown tall-grass vegetal index $index")
        }
    }

    private fun placeSavanna(
        context: PlacementContext,
        region: TallGrassRegion,
        random: WorldgenRandom,
        environment: SurfaceRuleEnvironment,
    ) {
        if (random.nextFloat() >= 1.0f / 5.0f) return
        val origin = inSquareAtHeight(context, random)
        if (biomeAt(environment, origin) !in SAVANNA_BIOMES) return
        placePatch(region, random, origin)
    }

    private fun placePlain(
        context: PlacementContext,
        region: TallGrassRegion,
        random: WorldgenRandom,
        environment: SurfaceRuleEnvironment,
    ) {
        val chunkOrigin = DecorationPos(context.chunk.chunkX * 16, 0, context.chunk.chunkZ * 16)
        val outerCount = if (PLAIN_COUNT_NOISE.getPositions(context, random, chunkOrigin).any()) 7 else 0
        repeat(outerCount) {
            if (random.nextFloat() < 1.0f / 32.0f) {
                val origin = inSquareAtHeight(context, random)
                if (biomeAt(environment, origin) in PLAIN_BIOMES) placePatch(region, random, origin)
            }
        }
    }

    private fun inSquareAtHeight(
        context: PlacementContext,
        random: WorldgenRandom,
    ): DecorationPos {
        val x = context.chunk.chunkX * 16 + random.nextInt(16)
        val z = context.chunk.chunkZ * 16 + random.nextInt(16)
        val y = context.height(x and 15, z and 15)
        return DecorationPos(x, y, z)
    }

    /** The patch at one origin, which is what `/place feature` runs for `patch_tall_grass`. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        placePatch(TallGrassRegion(chunk, null, null), random, origin)
    }

    private fun placePatch(
        region: TallGrassRegion,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        repeat(96) {
            val target =
                DecorationPos(
                    origin.x + random.nextInt(8) - random.nextInt(8),
                    origin.y + random.nextInt(4) - random.nextInt(4),
                    origin.z + random.nextInt(8) - random.nextInt(8),
                )
            if (target.y !in WorldHeight.MIN_Y until WorldHeight.MAX_Y ||
                !Blocks.isAir(region.get(target))
            ) {
                return@repeat
            }
            if (!Blocks.isAir(region.get(target.up()))) return@repeat
            if (region.get(target.down()) !in SUPPORTS_VEGETATION) return@repeat
            region.set(target, TALL_GRASS_LOWER_STATE_ID)
            region.set(target.up(), TALL_GRASS_UPPER_STATE_ID)
        }
    }

    private fun biomeAt(
        environment: SurfaceRuleEnvironment,
        position: DecorationPos,
    ): String = environment.biomeAt(position.x, position.y, position.z).substringAfter(':')

    private fun DecorationPos.up(): DecorationPos = copy(y = y + 1)

    private fun DecorationPos.down(): DecorationPos = copy(y = y - 1)

    private class TallGrassRegion(
        private val chunk: Chunk,
        private val externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
        private val blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)?,
    ) {
        private val externalWrites = HashMap<DecorationPos, Int>()

        fun get(position: DecorationPos): Int =
            when {
                position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y -> Blocks.AIR.stateId
                position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ ->
                    chunk.getBlockStateId(position.x and 15, position.y, position.z and 15)
                else ->
                    externalWrites[position] ?: externalBlockState?.invoke(position.x, position.y, position.z)
                        ?: Blocks.AIR.stateId
            }

        fun set(
            position: DecorationPos,
            stateId: Int,
        ) {
            if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return
            if (position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ) {
                chunk.setBlockStateId(position.x and 15, position.y, position.z and 15, stateId)
            } else {
                externalWrites[position] = stateId
            }
            blockWriteObserver?.invoke(position.x, position.y, position.z, stateId)
        }
    }

    private fun states(vararg names: String): Set<Int> =
        names
            .map { name ->
                Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")
            }.toSet()

    private val SUPPORTS_VEGETATION =
        states(
            "dirt",
            "coarse_dirt",
            "rooted_dirt",
            "mud",
            "muddy_mangrove_roots",
            "moss_block",
            "pale_moss_block",
            "grass_block",
            "podzol",
            "mycelium",
            "farmland",
        )
    private val PLAIN_COUNT_NOISE = NoiseThresholdCountPlacement(-0.8, belowNoise = 0, aboveNoise = 7)
    private val SAVANNA_BIOMES = setOf("savanna", "savanna_plateau")
    private val PLAIN_BIOMES =
        setOf(
            "plains",
            "sunflower_plains",
            "meadow",
            "cherry_grove",
            "sulfur_caves",
            "lush_caves",
            "dripstone_caves",
            "deep_dark",
        )
    private const val SAVANNA_INDEX = 8
    private const val PLAIN_INDEX = 26
    private const val VEGETAL_DECORATION_STEP = 9

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val TALL_GRASS_LOWER_STATE_ID = stateId("tall_grass")
    private val TALL_GRASS_UPPER_STATE_ID =
        Blocks.upperDoublePlantStateId(TALL_GRASS_LOWER_STATE_ID) ?: error("tall_grass has no upper half")
}
