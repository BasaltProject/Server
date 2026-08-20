package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** Exact step-9 placement for `patch_leaf_litter`. */
public object LeafLitterPatchDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, FEATURE_INDEX, VEGETAL_DECORATION_STEP)

        val context = PlacementContext(chunk)
        val region = PatchRegion(chunk, externalBlockState, blockWriteObserver)
        repeat(2) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = context.height(x and 15, z and 15, Chunk.HeightmapType.WORLD_SURFACE)
            if (environment.biomeAt(x, y, z).substringAfter(':') != BIOME) return@repeat
            placePatch(random, DecorationPos(x, y, z), region)
        }
    }

    private fun placePatch(
        random: WorldgenRandom,
        origin: DecorationPos,
        region: PatchRegion,
    ) {
        repeat(32) {
            val target =
                DecorationPos(
                    origin.x + random.nextInt(8) - random.nextInt(8),
                    origin.y + random.nextInt(4) - random.nextInt(4),
                    origin.z + random.nextInt(8) - random.nextInt(8),
                )
            if (!canPlace(region, target)) return@repeat

            // WeightedStateProvider has twelve unit-weight entries: amounts 1..3, each in
            // Direction.Plane.HORIZONTAL order NORTH, EAST, SOUTH, WEST.
            region.set(target, leafLitterStateId(random.nextInt(12)))
        }
    }

    private fun canPlace(
        region: PatchRegion,
        position: DecorationPos,
    ): Boolean {
        if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y || !Blocks.isAir(region.get(position))) return false
        if (position.y == WorldHeight.MIN_Y) return false
        return region.get(position.copy(y = position.y - 1)) == Blocks.GRASS_BLOCK.stateId
    }

    private fun leafLitterStateId(choice: Int): Int {
        val amountOffset = choice / 4
        val facingOffset = HORIZONTAL_FACING_STATE_OFFSETS[choice % 4]
        return DEFAULT_LEAF_LITTER_STATE_ID + facingOffset + amountOffset
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

    private val DEFAULT_LEAF_LITTER_STATE_ID = stateId("leaf_litter")
    private val HORIZONTAL_FACING_STATE_OFFSETS = intArrayOf(0, 12, 4, 8)
    private const val BIOME = "dark_forest"
    private const val FEATURE_INDEX = 77
    private const val VEGETAL_DECORATION_STEP = 9
}
