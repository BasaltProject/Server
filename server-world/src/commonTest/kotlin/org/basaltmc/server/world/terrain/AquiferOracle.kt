package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.world.densityfunction.BlendedNoise
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.noise.NormalNoise
import kotlin.test.Test

/**
 * The frozen Kotlin as the oracle for the aquifer. The synthetic noise parameters are derived from
 * the name exactly as in [NoiseRouterOracle], so the two dumps describe the same world.
 */
class AquiferOracle {
    private fun syntheticNoise(id: String): NormalNoise {
        var hash = -3750763034362895579L
        for (character in id) {
            hash = (hash xor character.code.toLong()) * 1099511628211L
        }
        val source = XoroshiroRandom(hash)
        return NormalNoise.create(source, -(id.length % 5 + 1), doubleArrayOf(1.0, 0.6, 0.3))
    }

    private fun base3d(): DensityFunction = BlendedNoise.createUnseeded(0.25, 0.125, 80.0, 160.0, 8.0)

    @Test
    fun dumpSubstances() {
        val router = NoiseRouterData.overworld(::syntheticNoise, base3d())
        // A sweep of near-zero densities rather than the router's own, whose extremes never reach
        // the barrier and flow paths this class exists for.
        val densities = doubleArrayOf(-0.001, -0.05, -0.2, -1.0, 0.5)
        val names = StringBuilder()
        val flags = StringBuilder()
        for (chunk in 0 until 2) {
            val aquifer = OverworldAquifer(router, 0x123456789L, chunk, -2, -64, 384)
            for (i in 0 until 64) {
                val x = chunk * 16 + (i * 7) % 16
                val z = -32 + (i * 11) % 16
                val y = -64 + (i * 6)
                val substance = aquifer.computeSubstance(x, y, z, densities[i % densities.size])
                names.append(substance?.block?.path ?: "stone").append(',')
                flags.append(if (aquifer.shouldScheduleFluidUpdate) '1' else '0')
            }
        }
        println("ORACLE aquifer.substances=$names")
        println("ORACLE aquifer.flags=$flags")
    }
}
