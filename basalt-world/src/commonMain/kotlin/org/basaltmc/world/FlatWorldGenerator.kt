package org.basaltmc.world

import org.basaltmc.registry.Blocks

/**
 * The single hardcoded layer stack every chunk gets: bedrock, two layers of dirt, grass on top.
 *
 * // parity: matches vanilla's classic superflat preset layers; real superflat worlds have a
 * configurable layer stack (`generator-settings`), which this does not read yet.
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
