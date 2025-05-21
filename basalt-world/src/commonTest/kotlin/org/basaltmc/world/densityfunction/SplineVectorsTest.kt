package org.basaltmc.world.densityfunction

import org.basaltmc.world.spline.CubicSplineBuilder
import kotlin.test.Test
import kotlin.test.assertEquals

/** Golden vectors captured by running the real decompiled 26.2 `CubicSpline`/`DensityFunctions.Spline` bytecode. */
class SplineVectorsTest {
    @Test
    fun matchesVanilla() {
        val input = YClampedGradient(0, 100, -1.0, 1.0)
        val coordinate = SplineCoordinate(input)
        val spline =
            CubicSplineBuilder<FunctionContext, SplineCoordinate>(coordinate)
                .addPoint(-1.0f, 0.0f, 0.0f)
                .addPoint(-0.5f, 5.0f, 2.0f)
                .addPoint(0.0f, 0.0f, 0.0f)
                .addPoint(1.0f, 10.0f, 0.0f)
                .build()
        val densityFunction = Spline(spline)

        val expected =
            mapOf(
                0 to 0.0,
                10 to 1.6639997959136963,
                25 to 5.0,
                40 to 1.8559999465942383,
                50 to 0.0,
                60 to 1.0399999618530273,
                75 to 5.0,
                90 to 8.960000038146973,
                100 to 10.0,
            )
        for ((y, value) in expected) {
            assertEquals(value, densityFunction.compute(FunctionContext(0, y, 0)), 1.0E-6)
        }
        assertEquals(-1.25, densityFunction.minValue(), 1.0E-6)
        assertEquals(10.0, densityFunction.maxValue())
    }

    @Test
    fun extrapolatesLinearlyPastTheEndpoints() {
        val wideInput = YClampedGradient(0, 100, -2.0, 2.0)
        val coordinate = SplineCoordinate(wideInput)
        val spline =
            CubicSplineBuilder<FunctionContext, SplineCoordinate>(coordinate)
                .addPoint(-1.0f, 0.0f, 1.0f)
                .addPoint(1.0f, 10.0f, 1.0f)
                .build()
        val densityFunction = Spline(spline)

        assertEquals(-1.0, densityFunction.compute(FunctionContext(0, 0, 0)))
        assertEquals(11.0, densityFunction.compute(FunctionContext(0, 100, 0)))
        assertEquals(-2.0, densityFunction.minValue())
        assertEquals(12.0, densityFunction.maxValue())
    }
}
