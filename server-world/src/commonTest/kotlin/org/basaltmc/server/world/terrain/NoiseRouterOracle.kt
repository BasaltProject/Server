package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.world.densityfunction.BlendedNoise
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.noise.NormalNoise
import kotlin.test.Test

/**
 * The frozen Kotlin as the oracle for the router graphs. The noise parameters vanilla loads from
 * datapack JSON are replaced by a synthetic set derived from the name, identical on both sides, so
 * what this pins is the shape of the graph and not Mojang's numbers.
 */
class NoiseRouterOracle {
    private fun syntheticNoise(id: String): NormalNoise {
        // FNV-1a over the name: a stable seed both implementations can reproduce.
        var hash = -3750763034362895579L
        for (character in id) {
            hash = (hash xor character.code.toLong()) * 1099511628211L
        }
        val source = XoroshiroRandom(hash)
        return NormalNoise.create(source, -(id.length % 5 + 1), doubleArrayOf(1.0, 0.6, 0.3))
    }

    private fun base3d(): DensityFunction = BlendedNoise.createUnseeded(0.25, 0.125, 80.0, 160.0, 8.0)

    private fun dump(label: String, function: DensityFunction) {
        println("ORACLE $label.min=" + function.minValue().toRawBits() + " max=" + function.maxValue().toRawBits())
        for (i in 0 until 6) {
            val context = FunctionContext(i * 137 - 400, i * 53 - 60, i * -211 + 300)
            println("ORACLE $label[$i]=" + function.compute(context).toRawBits())
        }
    }

    @Test
    fun dumpRouters() {
        val overworld = NoiseRouterData.overworld(::syntheticNoise, base3d())
        dump("ow.continents", overworld.continents)
        dump("ow.erosion", overworld.erosion)
        dump("ow.ridges", overworld.ridges)
        dump("ow.depth", overworld.depth)
        dump("ow.temperature", overworld.temperature)
        dump("ow.vegetation", overworld.vegetation)
        dump("ow.veinToggle", overworld.veinToggle)
        dump("ow.veinRidged", overworld.veinRidged)
        dump("ow.veinGap", overworld.veinGap)
        dump("ow.barrier", overworld.barrierNoise)
        dump("ow.final", overworld.finalDensity)
        dump("ow.surface", overworld.preliminarySurfaceLevel)

        val nether = NoiseRouterData.nether(::syntheticNoise, base3d())
        dump("nether.temperature", nether.temperature)
        dump("nether.vegetation", nether.vegetation)
        dump("nether.final", nether.finalDensity)

        val end = NoiseRouterData.end(base3d(), 0x5DEECE66DL)
        dump("end.erosion", end.erosion)
        dump("end.final", end.finalDensity)
    }
}
