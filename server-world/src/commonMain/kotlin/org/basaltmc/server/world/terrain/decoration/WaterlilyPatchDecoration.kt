package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** Exact step-9 placement for `patch_waterlily`. */
public object WaterlilyPatchDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, WATERLILY_INDEX, VEGETAL_DECORATION_STEP)
        val context = PlacementContext(chunk)
        val region = PatchRegion(chunk, externalBlockState, blockWriteObserver)
        repeat(4) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = context.height(x and 15, z and 15, Chunk.HeightmapType.WORLD_SURFACE)
            if (environment.biomeAt(x, y, z).substringAfter(':') !in BIOMES) return@repeat
            val origin = DecorationPos(x, y, z)
            repeat(10) {
                placeCandidate(region, random, origin)
            }
        }
    }

    /** The patch at one origin, which is what `/place feature` runs. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        val region = PatchRegion(chunk, null, null)
        repeat(10) { placeCandidate(region, random, origin) }
    }

    private fun placeCandidate(
        region: PatchRegion,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        val target =
            DecorationPos(
                origin.x + random.nextInt(8) - random.nextInt(8),
                origin.y + random.nextInt(4) - random.nextInt(4),
                origin.z + random.nextInt(8) - random.nextInt(8),
            )
        if (target.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y || !Blocks.isAir(region.get(target))) return
        val support = region.get(target.copy(y = target.y - 1))
        val supportPath = Blocks.byStateId(support)?.block?.path
        if (!Blocks.hasWaterFluid(support) && supportPath != "ice" && supportPath != "frosted_ice") return
        region.set(target, LILY_PAD_STATE_ID)
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
                externalWrites[position] = stateId
                chunk.setDecorationBlockStateId(position.x, position.y, position.z, stateId)
            }
            blockWriteObserver?.invoke(position.x, position.y, position.z, stateId)
        }
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val LILY_PAD_STATE_ID = stateId("lily_pad")
    private val BIOMES = setOf("mangrove_swamp", "swamp")
    private const val WATERLILY_INDEX = 65
    private const val VEGETAL_DECORATION_STEP = 9
}
