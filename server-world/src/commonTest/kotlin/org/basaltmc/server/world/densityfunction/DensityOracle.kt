package org.basaltmc.server.world.densityfunction

import org.basaltmc.server.random.XoroshiroRandom
import kotlin.test.Test

/**
 * The frozen Kotlin server as the second parity oracle for the two density functions whose output
 * is arithmetic rather than composition. Raw bits, because a decimal hides the last few and
 * worldgen notices them.
 */
class DensityOracle {
    @Test
    fun dump() {
        val blended = BlendedNoise(XoroshiroRandom(0L), 0.25, 0.125, 80.0, 160.0, 8.0)
        println("ORACLE blended.max=" + blended.maxValue().toRawBits())
        for (i in 0 until 8) {
            val context = FunctionContext(i * 37, i * 11 - 40, i * -23)
            val v = blended.compute(context)
            println("ORACLE blended[$i]=" + v.toRawBits() + " " + v)
        }

        val islands = EndIslandDensityFunction(1234567890L)
        for (i in 0 until 8) {
            val context = FunctionContext(i * 640, 64, i * -448)
            val v = islands.compute(context)
            println("ORACLE islands[$i]=" + v.toRawBits() + " " + v)
        }
    }
}
