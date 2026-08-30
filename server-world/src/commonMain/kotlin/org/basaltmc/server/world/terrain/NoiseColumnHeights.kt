package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.WorldHeight

/**
 * // parity: ChunkGenerator.getBaseHeight over `WORLD_SURFACE_WG`, at any column. A sampler per query
 * and deliberately no cache: [DensityChunkSampler] reuses one context, so sharing one across
 * concurrently generating chunks hands back wrong heights rather than crashing.
 */
public class NoiseColumnHeights(
    private val router: OverworldNoiseRouter,
) {
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

    /**
     * // parity: `OCEAN_FLOOR_WG`, the solid floor with no sea-level floor under it. [surfaceHeight]
     * lifts an open column to sea level because the aquifer fills it; this one must not, or a
     * shipwreck meant for the sea bed would settle on the surface of the water.
     */
    public fun oceanFloorHeight(
        x: Int,
        z: Int,
    ): Int {
        val sampler = samplerFor(x shr 4, z shr 4)
        val localX = x and 15
        val localZ = z and 15
        var y = WorldHeight.MAX_Y
        while (y >= WorldHeight.MIN_Y && sampler.sample(localX, y, localZ) <= 0.0) y--
        return y + 1
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
    ): DensityChunkSampler =
        DensityChunkSampler(
            router.finalDensity,
            chunkX,
            chunkZ,
            WorldHeight.MIN_Y,
            WorldHeight.HEIGHT,
            CELL_WIDTH,
            CELL_HEIGHT,
        )

    private companion object {
        const val CELL_WIDTH = 4
        const val CELL_HEIGHT = 8
        const val SEA_LEVEL = 63
    }
}
