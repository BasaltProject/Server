package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.DensityFunctions
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.densityfunction.SplineCoordinate
import kotlin.test.Test

/**
 * The frozen Kotlin as the oracle for the terrain splines. Each axis is a gradient in one
 * coordinate so a sample is a pure function of the position, and the raw bits are printed because a
 * decimal hides the last few and terrain notices them.
 */
class TerrainOracle {
    private fun axis(from: Double, to: Double): DensityFunction =
        DensityFunctions.yClampedGradient(-64, 320, from, to)

    @Test
    fun dump() {
        // Three different ramps so the three coordinates never coincide.
        val continents = SplineCoordinate(axis(-1.1, 1.0))
        val erosion = SplineCoordinate(axis(1.0, -1.0))
        val ridges = SplineCoordinate(axis(-1.0, 1.0))
        val weirdness = SplineCoordinate(axis(0.9, -0.9))

        val offset = TerrainProvider.overworldOffset(continents, erosion, ridges)
        val factor = TerrainProvider.overworldFactor(continents, erosion, weirdness, ridges)
        val jaggedness = TerrainProvider.overworldJaggedness(continents, erosion, weirdness, ridges)

        println("ORACLE offset.min=" + offset.minValue().toRawBits() + " max=" + offset.maxValue().toRawBits())
        println("ORACLE factor.min=" + factor.minValue().toRawBits() + " max=" + factor.maxValue().toRawBits())
        println("ORACLE jagged.min=" + jaggedness.minValue().toRawBits() + " max=" + jaggedness.maxValue().toRawBits())
        for (i in 0 until 10) {
            val y = -64 + i * 40
            val context = FunctionContext(0, y, 0)
            println("ORACLE offset[$i]=" + offset.sample(context).toRawBits())
            println("ORACLE factor[$i]=" + factor.sample(context).toRawBits())
            println("ORACLE jagged[$i]=" + jaggedness.sample(context).toRawBits())
        }
        println("ORACLE pav=" + TerrainProvider.peaksAndValleys(0.4f).toRawBits() +
            " " + TerrainProvider.peaksAndValleys(0.56666666f).toRawBits())
    }
}
