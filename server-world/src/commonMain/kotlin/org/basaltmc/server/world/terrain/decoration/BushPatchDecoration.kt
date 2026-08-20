package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** Exact step-9 placement for `patch_bush`. */
public object BushPatchDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, BUSH_INDEX, VEGETAL_DECORATION_STEP)
        if (random.nextFloat() >= 1.0f / 4.0f) return
        val context = PlacementContext(chunk)
        val region = PatchRegion(chunk, externalBlockState, blockWriteObserver)
        val x = chunk.chunkX * 16 + random.nextInt(16)
        val z = chunk.chunkZ * 16 + random.nextInt(16)
        val y = context.height(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING)
        if (environment.biomeAt(x, y, z).substringAfter(':') !in BIOMES) return
        placeAt(chunk, random, DecorationPos(x, y, z), region)
    }

    /**
     * The patch itself, at one origin: what the step calls once it has chosen a spot, and what
     * `/place feature` calls with the spot it was given.
     */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        placeAt(chunk, random, origin, PatchRegion(chunk, null, null))
    }

    private fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        region: PatchRegion,
    ) {
        repeat(24) {
            val target =
                DecorationPos(
                    origin.x + random.nextInt(6) - random.nextInt(6),
                    origin.y + random.nextInt(4) - random.nextInt(4),
                    origin.z + random.nextInt(6) - random.nextInt(6),
                )
            if (target.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y || !Blocks.isAir(region.get(target))) return@repeat
            if (target.y == WorldHeight.MIN_Y ||
                Blocks.byStateId(region.get(target.copy(y = target.y - 1)))?.block?.path !in SUPPORTS_VEGETATION
            ) {
                return@repeat
            }
            region.set(target, BUSH_STATE_ID)
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

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val BUSH_STATE_ID = stateId("bush")
    private val SUPPORTS_VEGETATION =
        setOf(
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
    private val BIOMES =
        setOf(
            "birch_forest",
            "forest",
            "frozen_river",
            "old_growth_birch_forest",
            "plains",
            "river",
            "windswept_forest",
            "windswept_gravelly_hills",
            "windswept_hills",
        )
    private const val BUSH_INDEX = 51
    private const val VEGETAL_DECORATION_STEP = 9
}
