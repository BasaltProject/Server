package org.basaltmc.server.world

import org.basaltmc.server.registry.Blocks

/**
 * Nothing at all: every chunk is air. A Basalt world type rather than a parity one, since vanilla's
 * closest is a superflat preset with an empty layer stack. It exists because a lobby wants an empty
 * canvas and no generation cost, which today means installing a plugin.
 *
 * [platformRadius] blocks of stone go under the world spawn so nobody falls out of a fresh void
 * world before building anything; 0 gives a truly empty one.
 */
public object VoidWorldGenerator {
    public const val DEFAULT_PLATFORM_RADIUS: Int = 4

    public fun generate(
        chunkX: Int,
        chunkZ: Int,
        spawnX: Int = 8,
        spawnY: Int = 64,
        spawnZ: Int = 8,
        platformRadius: Int = DEFAULT_PLATFORM_RADIUS,
        verticalBounds: ChunkHeight = ChunkHeight.OVERWORLD,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ, verticalBounds)
        if (platformRadius <= 0) return chunk
        // Only the chunks the platform actually reaches do any work; everything else stays air.
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val worldX = chunkX * 16 + x
                val worldZ = chunkZ * 16 + z
                if (worldX < spawnX - platformRadius || worldX > spawnX + platformRadius) continue
                if (worldZ < spawnZ - platformRadius || worldZ > spawnZ + platformRadius) continue
                chunk.setBlockStateId(x, spawnY - 1, z, Blocks.STONE.stateId)
            }
        }
        return chunk
    }
}
