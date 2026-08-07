package org.basaltmc.world.terrain

import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.terrain.surface.SurfaceBuilder
import org.basaltmc.world.terrain.surface.SurfaceRuleData
import org.basaltmc.world.terrain.surface.SurfaceRuleEnvironment

/**
 * Fills a [Chunk]'s solid/air shape from a real [OverworldNoiseRouter]'s `finalDensity`, sampled
 * through [DensityChunkSampler] (real vanilla's own cell-interpolation scheme, not per-block
 * evaluation, which earlier this project's own `BlendedNoise` work found to be too slow to be
 * viable), then runs the real [SurfaceRuleData.overworld] rule tree over it via [SurfaceBuilder]
 * (grass/dirt/gravel/deepslate/bedrock, real per-biome rules, not a fixed-depth stand-in). One
 * [OverworldNoiseRouter] should be built once per world (it is expensive to construct: dozens of
 * nested [org.basaltmc.world.densityfunction.DensityFunction] nodes) and reused across every chunk.
 *
 * [environment] is wrapped with the chunk's own just-computed heightmap for
 * [SurfaceRuleEnvironment.estimatedSurfaceHeight]/[SurfaceRuleEnvironment.isSteep]: both only need
 * this one already-generated chunk's own shape (real vanilla's own equivalents are chunk-scoped
 * too, not global), and a cheap array lookup here instead of [environment] re-scanning the density
 * function from scratch matters a lot: measured at roughly 80x more density evaluations per chunk
 * than [DensityChunkSampler]'s own cell grid needs, an easy trap to fall back into after already
 * solving it once for the base terrain shape.
 *
 * **Documented gaps, matching this project's own established scope for this pass:**
 * - No caves/aquifers/ore veins: [OverworldNoiseRouter.finalDensity] itself already omits them
 *   (see its own KDoc).
 * - No decoration (trees, ores, structures): separate, tracked work.
 */
public object RealTerrainGenerator {
    private const val CELL_WIDTH = 4
    private const val CELL_HEIGHT = 8

    public fun generate(
        router: OverworldNoiseRouter,
        environment: SurfaceRuleEnvironment,
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
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    if (sampler.sample(x, y, z) > 0.0) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                }
            }
        }
        val heightmap = chunk.heightmap()
        SurfaceBuilder.buildSurface(
            chunk,
            ChunkScopedSurfaceEnvironment(environment, heightmap),
            SurfaceRuleData.overworld(),
        )
        return chunk
    }
}

/**
 * // parity: how net.minecraft.world.level.levelgen.SurfaceRules.Context.SteepMaterialCondition
 * clamps its neighbor lookup to the chunk under construction (`Math.max(chunkBlockZ - 1, 0)` etc,
 * never crossing into a neighboring chunk).
 */
private class ChunkScopedSurfaceEnvironment(
    private val delegate: SurfaceRuleEnvironment,
    private val heightmap: IntArray,
) : SurfaceRuleEnvironment by delegate {
    override fun estimatedSurfaceHeight(
        x: Int,
        z: Int,
    ): Int = heightmap[(x and 15) + (z and 15) * 16]

    override fun isSteep(
        x: Int,
        z: Int,
    ): Boolean {
        val localX = x and 15
        val localZ = z and 15
        val zNorth = maxOf(localZ - 1, 0)
        val zSouth = minOf(localZ + 1, 15)
        if (heightAt(localX, zSouth) >= heightAt(localX, zNorth) + 4) return true
        val xWest = maxOf(localX - 1, 0)
        val xEast = minOf(localX + 1, 15)
        return heightAt(xWest, localZ) >= heightAt(xEast, localZ) + 4
    }

    private fun heightAt(
        localX: Int,
        localZ: Int,
    ): Int = heightmap[localX + localZ * 16]
}
