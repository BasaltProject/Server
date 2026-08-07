package org.basaltmc.world.terrain

import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight

/**
 * Fills a [Chunk]'s solid/air shape from a real [OverworldNoiseRouter]'s `finalDensity`, sampled
 * through [DensityChunkSampler] (real vanilla's own cell-interpolation scheme, not per-block
 * evaluation, which earlier this project's own `BlendedNoise` work found to be too slow to be
 * viable). One [OverworldNoiseRouter] should be built once per world (it is expensive to
 * construct: dozens of nested [org.basaltmc.world.densityfunction.DensityFunction] nodes) and
 * reused across every chunk.
 *
 * **Documented gaps, matching this project's own established scope for this pass:**
 * - No caves/aquifers/ore veins: [OverworldNoiseRouter.finalDensity] itself already omits them
 *   (see its own KDoc).
 * - Surface layering (grass/dirt/stone) is a fixed depth, not real `SurfaceRules`: real per-biome
 *   surface rules are separate, tracked work.
 * - No decoration (trees, ores, structures): also separate, tracked work.
 */
public object RealTerrainGenerator {
    private const val CELL_WIDTH = 4
    private const val CELL_HEIGHT = 8
    private const val SURFACE_DEPTH = 4

    public fun generate(
        router: OverworldNoiseRouter,
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
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
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                generateColumn(chunk, sampler, x, z)
            }
        }
        return chunk
    }

    private fun generateColumn(
        chunk: Chunk,
        sampler: DensityChunkSampler,
        x: Int,
        z: Int,
    ) {
        for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
            if (sampler.sample(x, y, z) > 0.0) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
        }
        chunk.setBlockStateId(x, WorldHeight.MIN_Y, z, Blocks.BEDROCK.stateId)

        var depth = 0
        for (y in WorldHeight.MAX_Y downTo WorldHeight.MIN_Y + 1) {
            if (chunk.getBlockStateId(x, y, z) != Blocks.STONE.stateId) {
                if (depth > 0) break
                continue
            }
            chunk.setBlockStateId(x, y, z, if (depth == 0) Blocks.GRASS_BLOCK.stateId else Blocks.DIRT.stateId)
            depth++
            if (depth >= SURFACE_DEPTH) break
        }
    }
}
