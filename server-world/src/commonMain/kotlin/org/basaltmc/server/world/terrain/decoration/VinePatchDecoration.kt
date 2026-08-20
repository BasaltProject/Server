package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** Exact step-9 placement for the shared jungle `vines` feature. */
public object VinePatchDecoration {
    internal fun placeCaveAt(
        chunk: Chunk,
        position: DecorationPos,
    ) {
        placeAt(PatchRegion(chunk, null, null), position)
    }

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
        val region = PatchRegion(chunk, externalBlockState, blockWriteObserver)

        repeat(127) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val position = DecorationPos(x, 64 + random.nextInt(37), z)
            if (environment.biomeAt(position.x, position.y, position.z).substringAfter(':') !in BIOMES) return@repeat
            placeAt(region, position)
        }
    }

    private fun placeAt(
        region: PatchRegion,
        position: DecorationPos,
    ) {
        if (!Blocks.isAir(region.get(position))) return
        for (direction in DIRECTIONS) {
            val neighbourPosition =
                position.copy(
                    x = position.x + direction.face.stepX,
                    y = position.y + direction.face.stepY,
                    z = position.z + direction.face.stepZ,
                )
            val neighbour = region.get(neighbourPosition)
            val attachmentFace = direction.face.opposite
            val acceptable =
                BlockShapes.isFaceSturdy(neighbour, attachmentFace) ||
                    BlockShapes.faceCovers(
                        neighbour,
                        attachmentFace,
                        0,
                        BlockShapes.UNITS,
                        0,
                        BlockShapes.UNITS,
                    )
            if (!acceptable) continue
            region.set(position, DEFAULT_VINE_STATE_ID - direction.falseBit)
            return
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

    private data class Direction(
        val face: BlockFace,
        val falseBit: Int,
    )

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val DEFAULT_VINE_STATE_ID = stateId("vine")
    private val DIRECTIONS =
        listOf(
            Direction(BlockFace.UP, 2),
            Direction(BlockFace.NORTH, 8),
            Direction(BlockFace.SOUTH, 4),
            Direction(BlockFace.WEST, 1),
            Direction(BlockFace.EAST, 16),
        )
    private val BIOMES = setOf("bamboo_jungle", "jungle", "sparse_jungle")
    private const val FEATURE_INDEX = 91
    private const val VEGETAL_DECORATION_STEP = 9
}
