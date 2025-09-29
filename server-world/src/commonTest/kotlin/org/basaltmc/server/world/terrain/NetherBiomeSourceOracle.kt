package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.densityfunction.DensityFunctions
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the five-point Nether biome resolution. */
class NetherBiomeSourceOracle {
    @Test
    fun dump() {
        // Sweeping the two axes the Nether actually uses walks the whole parameter tree, including
        // the offsets that bias warped forest and basalt deltas.
        val grid = StringBuilder()
        for (temperatureStep in -10..10) {
            for (vegetationStep in -10..10) {
                val source =
                    NetherBiomeSource(
                        DensityFunctions.constant(temperatureStep / 10.0),
                        DensityFunctions.constant(vegetationStep / 10.0),
                    )
                grid.append(source.biomeAt(0, 64, 0).id.path.first())
            }
            grid.append('|')
        }
        println("ORACLE grid=$grid")

        // The zoomed lookup rides a Voronoi over the quart grid, so it varies with block position
        // even when the noise does not.
        // A y gradient makes the answer depend on which quart the Voronoi lands on, which a
        // constant noise would hide.
        val zoomed =
            NetherBiomeSource(
                DensityFunctions.yClampedGradient(0, 128, -1.0, 1.0),
                DensityFunctions.yClampedGradient(0, 40, 1.0, -1.0),
                biomeZoomSeed = 987654321L,
            )
        // Which quart the Voronoi picks is pinned by the zoomer's own test; what this covers is
        // that the zoomed lookup is wired to it and answers with real biomes.
        val walk = StringBuilder()
        for (index in 0 until 96) {
            val x = index * 37 - 900
            val y = 20 + index / 4
            val z = index * -53 + 400
            walk.append(zoomed.blockBiomeAt(x, y, z).id.path.first())
        }
        println("ORACLE zoomed=$walk")
    }
}
