package org.basaltmc.server.world.climate

import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.FunctionContext
import kotlin.test.Test
import kotlin.test.assertEquals

class SamplerTest {
    @Test
    fun sampleConvertsQuartCoordinatesAndQuantizesEachAxis() {
        var lastContext: FunctionContext? = null
        val constant =
            object : DensityFunction {
                override fun compute(context: FunctionContext): Double {
                    lastContext = context
                    return 0.5
                }

                override fun minValue(): Double = 0.5

                override fun maxValue(): Double = 0.5
            }
        val sampler = Sampler(constant, constant, constant, constant, constant, constant)

        val result = sampler.sample(quartX = 2, quartY = 3, quartZ = 4)
        val context = requireNotNull(lastContext)

        assertEquals(8, context.blockX)
        assertEquals(12, context.blockY)
        assertEquals(16, context.blockZ)
        assertEquals(TargetPoint(5000L, 5000L, 5000L, 5000L, 5000L, 5000L), result)
    }
}
