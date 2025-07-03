package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment

/** Exact step-9 placement for `patch_pumpkin`. */
public object PumpkinPatchDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, PUMPKIN_INDEX, VEGETAL_DECORATION_STEP)
        if (random.nextFloat() >= 1.0f / 300.0f) return
        val context = PlacementContext(chunk)
        val region = PatchRegion(chunk, externalBlockState, blockWriteObserver)
        val x = chunk.chunkX * 16 + random.nextInt(16)
        val z = chunk.chunkZ * 16 + random.nextInt(16)
        val y = context.height(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING)
        if (environment.biomeAt(x, y, z).substringAfter(':') !in BIOMES) return
        placeAt(random, DecorationPos(x, y, z), region)
    }

    /** The patch at one origin: what the step calls after choosing a spot, and what `/place feature` calls. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        placeAt(random, origin, PatchRegion(chunk, null, null))
    }

    private fun placeAt(
        random: WorldgenRandom,
        origin: DecorationPos,
        region: PatchRegion,
    ) {
        repeat(96) {
            val target =
                DecorationPos(
                    origin.x + random.nextInt(8) - random.nextInt(8),
                    origin.y + random.nextInt(4) - random.nextInt(4),
                    origin.z + random.nextInt(8) - random.nextInt(8),
                )
            if (target.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y || !Blocks.isAir(region.get(target))) return@repeat
            if (target.y == WorldHeight.MIN_Y ||
                Blocks.byStateId(region.get(target.copy(y = target.y - 1)))?.block != Blocks.GRASS_BLOCK.block
            ) {
                return@repeat
            }
            region.set(target, Blocks.PUMPKIN.stateId)
        }
    }

    private class PatchRegion(
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
            if (position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ) {
                chunk.setBlockStateId(position.x and 15, position.y, position.z and 15, stateId)
            } else {
                setExternalDecorationBlockStateId(chunk, externalWrites, externalBlockState, position, stateId)
            }
            blockWriteObserver?.invoke(position.x, position.y, position.z, stateId)
        }
    }

    private val BIOMES =
        setOf(
            "badlands",
            "bamboo_jungle",
            "beach",
            "birch_forest",
            "cold_ocean",
            "dark_forest",
            "deep_cold_ocean",
            "deep_dark",
            "deep_frozen_ocean",
            "deep_lukewarm_ocean",
            "deep_ocean",
            "desert",
            "dripstone_caves",
            "eroded_badlands",
            "flower_forest",
            "forest",
            "frozen_ocean",
            "frozen_river",
            "grove",
            "ice_spikes",
            "jungle",
            "lukewarm_ocean",
            "ocean",
            "old_growth_birch_forest",
            "old_growth_pine_taiga",
            "old_growth_spruce_taiga",
            "pale_garden",
            "plains",
            "river",
            "savanna",
            "savanna_plateau",
            "snowy_beach",
            "snowy_plains",
            "snowy_slopes",
            "snowy_taiga",
            "sparse_jungle",
            "stony_shore",
            "sunflower_plains",
            "swamp",
            "taiga",
            "warm_ocean",
            "windswept_forest",
            "windswept_gravelly_hills",
            "windswept_hills",
            "windswept_savanna",
            "wooded_badlands",
        )
    private const val PUMPKIN_INDEX = 82
    private const val VEGETAL_DECORATION_STEP = 9
}
