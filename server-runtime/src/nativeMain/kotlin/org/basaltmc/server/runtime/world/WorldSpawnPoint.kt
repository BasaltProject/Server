package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.hasFluid
import org.basaltmc.server.world.Chunk

/**
 * // parity: MinecraftServer.setInitialSpawn plus PlayerSpawnFinder. Where the climate search lands
 * is only the chunk; the block itself comes from a spiral of up to eleven by eleven chunks around
 * it, and the first column in the first chunk that can hold a player wins.
 */
internal object WorldSpawnPoint {
    /**
     * The spawn block, or null when nothing in reach can hold a player. [chunkAt] must return a
     * fully generated chunk.
     */
    fun find(
        spawnChunkX: Int,
        spawnChunkZ: Int,
        chunkAt: (chunkX: Int, chunkZ: Int) -> Chunk,
    ): Triple<Int, Int, Int>? {
        var offsetX = 0
        var offsetZ = 0
        var stepX = 0
        var stepZ = -1
        repeat(SPIRAL_CHUNKS * SPIRAL_CHUNKS) {
            if (offsetX in -REACH..REACH && offsetZ in -REACH..REACH) {
                spawnInChunk(chunkAt(spawnChunkX + offsetX, spawnChunkZ + offsetZ))?.let { return it }
            }
            // parity: the square spiral of `setInitialSpawn`, which turns at the diagonals.
            if (offsetX == offsetZ ||
                (offsetX < 0 && offsetX == -offsetZ) ||
                (offsetX > 0 && offsetX == 1 - offsetZ)
            ) {
                val turned = stepX
                stepX = -stepZ
                stepZ = turned
            }
            offsetX += stepX
            offsetZ += stepZ
        }
        return null
    }

    /** // parity: PlayerSpawnFinder.getSpawnPosInChunk, which walks x first and takes the first hit. */
    private fun spawnInChunk(chunk: Chunk): Triple<Int, Int, Int>? {
        for (localX in 0 until 16) {
            for (localZ in 0 until 16) {
                respawnPos(chunk, localX, localZ)?.let { return it }
            }
        }
        return null
    }

    /**
     * // parity: PlayerSpawnFinder.getLevelRespawnPos. A column whose world surface stands above both the
     * motion-blocking top and the ocean floor is refused outright; otherwise it is walked down and the
     * first full upward face under open air wins, unless fluid gets there first.
     */
    private fun respawnPos(
        chunk: Chunk,
        localX: Int,
        localZ: Int,
    ): Triple<Int, Int, Int>? {
        val minY = chunk.verticalBounds.minY
        val topY = chunk.heightAt(localX, localZ, Chunk.HeightmapType.MOTION_BLOCKING)
        if (topY < minY) return null
        val surface = chunk.heightAt(localX, localZ, Chunk.HeightmapType.WORLD_SURFACE)
        if (surface <= topY && surface > chunk.heightAt(localX, localZ, Chunk.HeightmapType.OCEAN_FLOOR)) return null
        val worldX = chunk.chunkX * 16 + localX
        val worldZ = chunk.chunkZ * 16 + localZ
        for (y in topY + 1 downTo minY) {
            val state = chunk.getBlockStateId(localX, y, localZ)
            if (Blocks.hasFluid(state)) return null
            if (BlockShapes.isCollisionFaceFull(state, BlockFace.UP)) return Triple(worldX, y + 1, worldZ)
        }
        return null
    }

    private const val SPIRAL_CHUNKS = 11
    private const val REACH = 5
}
