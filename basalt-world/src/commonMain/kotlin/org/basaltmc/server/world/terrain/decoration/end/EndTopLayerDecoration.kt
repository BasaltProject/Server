package org.basaltmc.server.world.terrain.decoration.end

import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.TheEndBiomeSource

/** // parity: `end_platform`, the End's fixed TOP_LAYER_MODIFICATION feature. */
public object EndTopLayerDecoration {
    public fun decorate(
        chunk: Chunk,
        biomeSource: TheEndBiomeSource,
    ) {
        if (biomeSource.biomeAtQuart(ORIGIN_X shr 2, ORIGIN_Y shr 2, ORIGIN_Z shr 2) != Biomes.THE_END) return
        if (chunk.chunkX != ORIGIN_X.floorDiv(16) || chunk.chunkZ !in AFFECTED_CHUNK_Z) return

        for (z in maxOf(chunk.chunkZ * 16, ORIGIN_Z - RADIUS)..minOf(chunk.chunkZ * 16 + 15, ORIGIN_Z + RADIUS)) {
            for (x in ORIGIN_X - RADIUS..ORIGIN_X + RADIUS) {
                setBlock(chunk, x, ORIGIN_Y - 1, z, Blocks.OBSIDIAN.stateId)
                for (y in ORIGIN_Y..ORIGIN_Y + 2) setBlock(chunk, x, y, z, Blocks.AIR.stateId)
            }
        }
    }

    private fun setBlock(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        if (y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) return
        chunk.setBlockStateId(x and 15, y, z and 15, stateId)
    }

    private val AFFECTED_CHUNK_Z = (ORIGIN_Z - RADIUS).floorDiv(16)..(ORIGIN_Z + RADIUS).floorDiv(16)
    private const val ORIGIN_X = 100
    private const val ORIGIN_Y = 49
    private const val ORIGIN_Z = 0
    private const val RADIUS = 2
}
