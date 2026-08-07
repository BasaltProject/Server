package org.basaltmc.world

import org.basaltmc.registry.Blocks

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
    private const val SURFACE_Y = WorldHeight.MIN_Y + 3

    public fun generate(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                chunk.setBlockStateId(x, WorldHeight.MIN_Y, z, Blocks.BEDROCK.stateId)
                chunk.setBlockStateId(x, WorldHeight.MIN_Y + 1, z, Blocks.DIRT.stateId)
                chunk.setBlockStateId(x, WorldHeight.MIN_Y + 2, z, Blocks.DIRT.stateId)
                chunk.setBlockStateId(x, SURFACE_Y, z, Blocks.GRASS_BLOCK.stateId)
            }
        }
        return chunk
    }
}
