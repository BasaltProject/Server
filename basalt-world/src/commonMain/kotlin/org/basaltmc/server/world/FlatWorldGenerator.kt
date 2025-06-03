package org.basaltmc.server.world

import org.basaltmc.server.registry.Blocks

/**
 * The single hardcoded layer stack every chunk gets: bedrock, two layers of dirt, grass on top.
 *
 * // parity: matches vanilla's default flat preset layers, confirmed live rather than assumed: a
 * real 26.2 server with `level-type=minecraft:flat` and no explicit `generator-settings` was run,
 * its saved region read back with this project's own `AnvilRegion`/`ChunkNbt`, and every one of the
 * 98,304 blocks in chunk (0,0) matched this generator's output exactly (bedrock/dirt/dirt/grass at
 * the bottom, air above). Real flat worlds have a configurable layer stack
 * (`generator-settings`), which this does not read yet; only the default preset is replicated.
 */
public object FlatWorldGenerator {
    public fun generate(
        chunkX: Int,
        chunkZ: Int,
        verticalBounds: ChunkHeight = ChunkHeight.OVERWORLD,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ, verticalBounds)
        val minY = verticalBounds.minY
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                chunk.setBlockStateId(x, minY, z, Blocks.BEDROCK.stateId)
                chunk.setBlockStateId(x, minY + 1, z, Blocks.DIRT.stateId)
                chunk.setBlockStateId(x, minY + 2, z, Blocks.DIRT.stateId)
                chunk.setBlockStateId(x, minY + 3, z, Blocks.GRASS_BLOCK.stateId)
            }
        }
        return chunk
    }
}
