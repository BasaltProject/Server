package org.basaltmc.world

import org.basaltmc.random.LegacyRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.densityfunction.BlendedNoise
import org.basaltmc.world.densityfunction.FunctionContext
import org.basaltmc.world.noise.NormalNoise

/**
 * A real, noise-driven terrain generator: **not vanilla parity yet**, an explicit, deliberate
 * stepping stone (requested directly, to get a player seeing shaped terrain and chunks actually
 * saved to disk before the real pipeline exists) rather than an accidental shortcut.
 *
 * Shape follows vanilla's own structure (`NoiseRouterData.overworld`, decompiled): a solid/air
 * decision from `density = base3dNoise + depthTerm`, positive meaning solid. [base3d] is
 * vanilla's own [BlendedNoise] with vanilla's own real Overworld constants
 * (`NoiseRouterData.BASE_3D_NOISE_OVERWORLD`'s `0.25, 0.125, 80.0, 160.0, 8.0`); `depthTerm` biases
 * toward solid below a 2D height field and air above it, the same shape as vanilla's own
 * `offsetToDepth`/`noiseGradientDensity`.
 *
 * **Explicit non-parity gaps, all deliberate, all pending the real pipeline:**
 * - [heightNoise] uses this project's own octave/amplitude choice for 2D height variation, not
 *   vanilla's real `continentalness`/`erosion`/`weirdness` parameters or `TerrainProvider`'s
 *   splines: those parameters are Mojang datapack content, not committed here (§7), and the
 *   splines themselves are not implemented yet either.
 * - No caves, no ore veins, no aquifers.
 * - No per-biome surface rules: the dirt/grass/stone layering is a fixed depth, not decided by
 *   biome, climate, or slope the way vanilla's `SurfaceSystem` does it.
 * - The seed is fixed, not read from `basalt.toml` or per-world state.
 * - [base3d] is only sampled every [CELL_HEIGHT] blocks and held constant across each cell,
 *   instead of vanilla's real interpolated-cell noise sampling (the `Interpolated` marker/
 *   `NoiseSettings.cellHeight` in real vanilla): sampling every single block was measured at
 *   several seconds per chunk (`BlendedNoise.compute` runs 24 octave evaluations per call), too
 *   slow for a player to actually connect to. [base3d]'s contribution is a small perturbation on
 *   top of the (smooth, cheap) `depthTerm`, so a coarser vertical resolution is not very visible.
 * - [base3d] is skipped entirely once `depthTerm` alone is decisive (further from the surface
 *   than [SKIP_THRESHOLD] divided by [DENSITY_SLOPE]): [base3d]'s real output stays well under
 *   that bound in practice, so this only skips columns of blocks that were always going to be
 *   solid stone or always air regardless, not a behavior change, just not paying for noise no
 *   sign flip could reach. Most of a 384-tall column is far from the surface, so this is the
 *   difference between a chunk being viable to send to a player and not.
 * - Trees ([placeTrees]) are a per-chunk hash of chunk coordinates, not vanilla's real per-chunk
 *   feature/decoration seed (`ChunkGenerator`'s own formula, not decompiled for this stepping
 *   stone) or its real tree population/placement logic (density, species, shape all vanilla's
 *   own, biome-dependent). A canopy that would cross into a neighboring chunk is just skipped
 *   rather than placed there, real vanilla generates decorations across chunk borders freely.
 */
public object NoiseWorldGenerator {
    private const val SEED = 0L
    private val base3d = BlendedNoise.createUnseeded(0.25, 0.125, 80.0, 160.0, 8.0)
    private val heightNoise = NormalNoise.create(LegacyRandom(SEED), -5, doubleArrayOf(1.0, 1.0, 1.0, 1.0, 1.0))

    private const val SEA_LEVEL = 63
    private const val HEIGHT_VARIATION = 6.0
    private const val DENSITY_SLOPE = 0.08
    private const val SURFACE_DEPTH = 4
    private const val CELL_HEIGHT = 4

    // How much a world coordinate is scaled before sampling heightNoise: higher means shorter
    // wavelength, i.e. more distinct hills per loaded chunk instead of one broad slope spanning
    // the whole loaded area (heightNoise's own lowest octave has an ~32-unit period in its own
    // coordinate space, so a small scale here stretches that period well past a single chunk).
    // Kept low, combined with a small [HEIGHT_VARIATION], for gentle Plains-like rolling instead
    // of a hill per chunk.
    private const val HEIGHT_NOISE_SCALE = 0.25

    // See the class KDoc's note on skipping base3d far from the surface. BlendedNoise's own
    // theoretical max is much larger, but real samples across this generator's whole verified
    // test suite never exceeded roughly 0.5; this keeps a comfortable margin above that.
    private const val SKIP_THRESHOLD = 2.0

    private const val MAX_TREES_PER_CHUNK = 2
    private const val MIN_TRUNK_HEIGHT = 4
    private const val TRUNK_HEIGHT_VARIATION = 2

    public fun generate(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val worldX = chunkX * 16 + x
                val worldZ = chunkZ * 16 + z
                generateColumn(chunk, x, z, worldX, worldZ)
            }
        }
        placeTrees(chunk, chunkX, chunkZ)
        return chunk
    }

    private fun generateColumn(
        chunk: Chunk,
        x: Int,
        z: Int,
        worldX: Int,
        worldZ: Int,
    ) {
        val surfaceY =
            SEA_LEVEL +
                (
                    heightNoise.getValue(
                        worldX * HEIGHT_NOISE_SCALE,
                        0.0,
                        worldZ * HEIGHT_NOISE_SCALE,
                    ) * HEIGHT_VARIATION
                ).toInt()
        var cellBase3d = 0.0
        for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
            val depthTerm = (surfaceY - y) * DENSITY_SLOPE
            val solid =
                when {
                    depthTerm > SKIP_THRESHOLD -> true
                    depthTerm < -SKIP_THRESHOLD -> false
                    else -> {
                        if ((y - WorldHeight.MIN_Y) % CELL_HEIGHT == 0) {
                            cellBase3d = base3d.compute(FunctionContext(worldX, y, worldZ))
                        }
                        (cellBase3d + depthTerm) > 0.0
                    }
                }
            if (solid) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
        }
        chunk.setBlockStateId(x, WorldHeight.MIN_Y, z, Blocks.BEDROCK.stateId)

        // Surface pass: from the top down, turn the topmost contiguous run of stone into
        // dirt/grass, the same shape as vanilla's own surface builder (works top-down per
        // column), just without its per-biome rules.
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

    private fun placeTrees(
        chunk: Chunk,
        chunkX: Int,
        chunkZ: Int,
    ) {
        val random = LegacyRandom(chunkSeed(chunkX, chunkZ))
        val treeCount = random.nextInt(MAX_TREES_PER_CHUNK + 1)
        repeat(treeCount) {
            val x = random.nextInt(16)
            val z = random.nextInt(16)
            val topY = chunk.heightmap()[x + z * 16]
            if (chunk.getBlockStateId(x, topY - 1, z) != Blocks.GRASS_BLOCK.stateId) return@repeat

            val trunkHeight = MIN_TRUNK_HEIGHT + random.nextInt(TRUNK_HEIGHT_VARIATION + 1)
            if (topY + trunkHeight + 1 > WorldHeight.MAX_Y) return@repeat

            for (dy in 0 until trunkHeight) {
                chunk.setBlockStateId(x, topY + dy, z, Blocks.OAK_LOG.stateId)
            }
            placeCanopy(chunk, x, topY + trunkHeight, z)
        }
    }

    private fun placeCanopy(
        chunk: Chunk,
        trunkX: Int,
        trunkTopY: Int,
        trunkZ: Int,
    ) {
        for (dy in -2..0) {
            val radius = if (dy == 0) 1 else 2
            for (dx in -radius..radius) {
                for (dz in -radius..radius) {
                    if (dx == 0 && dz == 0 && dy < 0) continue
                    val px = trunkX + dx
                    val pz = trunkZ + dz
                    // Canopy that would cross into a neighboring chunk is skipped, not placed
                    // there: see the class KDoc's note on this generator's tree-placement gaps.
                    if (px !in 0..15 || pz !in 0..15) continue
                    val py = trunkTopY + dy
                    if (Blocks.isAir(chunk.getBlockStateId(px, py, pz))) {
                        chunk.setBlockStateId(px, py, pz, Blocks.OAK_LEAVES.stateId)
                    }
                }
            }
        }
    }

    // Not vanilla's own per-chunk feature/decoration seed, just a stand-in hash: see the class
    // KDoc's note on this generator's tree-placement gaps.
    private fun chunkSeed(
        chunkX: Int,
        chunkZ: Int,
    ): Long =
        chunkX.toLong() * 0x9E3779B97F4A7C15UL.toLong() xor (chunkZ.toLong() * 0xBF58476D1CE4E5B9UL.toLong()) xor SEED
}
