package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.world.densityfunction.BlendedNoise
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.noise.NormalNoise
import kotlin.test.Test

/**
 * The frozen Kotlin as the oracle for the Overworld biome source. The noise parameters are the same
 * synthetic set the router oracle uses, so what this pins is the sampling and the climate search,
 * not Mojang's numbers.
 */
class OverworldBiomeSourceOracle {
    private fun syntheticNoise(id: String): NormalNoise {
        var hash = -3750763034362895579L
        for (character in id) {
            hash = (hash xor character.code.toLong()) * 1099511628211L
        }
        val source = XoroshiroRandom(hash)
        return NormalNoise.create(source, -(id.length % 5 + 1), doubleArrayOf(1.0, 0.6, 0.3))
    }

    private fun base3d(): DensityFunction = BlendedNoise.createUnseeded(0.25, 0.125, 80.0, 160.0, 8.0)

    private fun report(
        name: String,
        source: OverworldBiomeSource,
        zoomed: Boolean,
    ) {
        var checksum = -3750763034362895579L
        val counts = HashMap<String, Int>()
        for (index in 0 until 4096) {
            val x = (index * 37).mod(2048) - 1024
            val z = (index * 91).mod(2048) - 1024
            val y = (index * 13).mod(256) - 64
            val biome = if (zoomed) source.blockBiomeAt(x, y, z) else source.biomeAt(x, y, z)
            val path = biome.id.path
            counts[path] = (counts[path] ?: 0) + 1
            for (character in path) checksum = (checksum xor character.code.toLong()) * 1099511628211L
        }
        val summary = counts.keys.sorted().joinToString(",") { "$it=${counts[it]}" }
        println("ORACLE $name distinct=${counts.size} checksum=$checksum counts=$summary")
    }

    @Test
    fun dump() {
        val router = NoiseRouterData.overworld(::syntheticNoise, base3d())
        report("direct", OverworldBiomeSource(router), zoomed = false)
        report("zoomed", OverworldBiomeSource(router, biomeZoomSeed = 1729L), zoomed = true)
    }
}
