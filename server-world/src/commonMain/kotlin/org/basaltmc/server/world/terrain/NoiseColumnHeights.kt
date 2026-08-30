package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.WorldHeight

/**
 * // parity: ChunkGenerator.getBaseHeight over `WORLD_SURFACE_WG`, at any column rather than only
 * inside the chunk being generated.
 *
 * A structure has to know how high the ground is at its corners before it has a chunk to ask, so
 * this samples the density directly. One sampler per chunk column touched, kept because a
 * structure asks about the same few columns many times over.
 */
public class NoiseColumnHeights(
    private val router: OverworldNoiseRouter,
    private val capacity: Int = DEFAULT_CAPACITY,
) {
    private val samplers = LinkedHashMap<Long, DensityChunkSampler>()

    /** The first free block above the ground at this column. */
    public fun surfaceHeight(
        x: Int,
        z: Int,
    ): Int {
        val sampler = samplerFor(x shr 4, z shr 4)
        val localX = x and 15
        val localZ = z and 15
        var y = WorldHeight.MAX_Y
        while (y >= WorldHeight.MIN_Y && sampler.sample(localX, y, localZ) <= 0.0) y--
        // The Overworld's global aquifer fills an open column through sea level, so the world
        // surface never reads below it even where the solid floor does.
        return (y + 1).coerceAtLeast(SEA_LEVEL)
    }

    /** Whether the density says this block is solid, which is what a portal's corners are tested against. */
    public fun isSolid(
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        if (y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return false
        return samplerFor(x shr 4, z shr 4).sample(x and 15, y, z and 15) > 0.0
    }

    private fun samplerFor(
        chunkX: Int,
        chunkZ: Int,
    ): DensityChunkSampler {
        val key = (chunkX.toLong() shl 32) or (chunkZ.toLong() and 0xFFFFFFFFL)
        samplers[key]?.let { return it }
        val sampler =
            DensityChunkSampler(
                router.finalDensity,
                chunkX,
                chunkZ,
                WorldHeight.MIN_Y,
                WorldHeight.HEIGHT,
                CELL_WIDTH,
                CELL_HEIGHT,
            )
        samplers[key] = sampler
        if (samplers.size > capacity) samplers.remove(samplers.keys.first())
        return sampler
    }

    private companion object {
        const val CELL_WIDTH = 4
        const val CELL_HEIGHT = 8
        const val SEA_LEVEL = 63
        const val DEFAULT_CAPACITY = 64
    }
}
