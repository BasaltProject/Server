package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.FunctionContext
import kotlin.test.Test
import kotlin.test.assertEquals

class TheEndBiomeSourceTest {
    @Test
    fun central4096ChunkRadiusAlwaysUsesTheEnd() {
        val source = TheEndBiomeSource(ConstantDensity(1.0))

        assertEquals(Biomes.THE_END, source.biomeAtQuart(0, 0, 0))
        assertEquals(Biomes.THE_END, source.biomeAtQuart(64 * 4, 20, 0))
        assertEquals(Biomes.END_HIGHLANDS, source.biomeAtQuart(65 * 4, 20, 0))
    }

    @Test
    fun outerBiomesUseTheExactThresholds() {
        val quartX = 65 * 4

        assertEquals(Biomes.END_HIGHLANDS, source(0.2500001).biomeAtQuart(quartX, 0, 0))
        assertEquals(Biomes.END_MIDLANDS, source(0.25).biomeAtQuart(quartX, 0, 0))
        assertEquals(Biomes.END_MIDLANDS, source(-0.0625).biomeAtQuart(quartX, 0, 0))
        assertEquals(Biomes.END_BARRENS, source(-0.0625001).biomeAtQuart(quartX, 0, 0))
        assertEquals(Biomes.END_BARRENS, source(-0.21875).biomeAtQuart(quartX, 0, 0))
        assertEquals(Biomes.SMALL_END_ISLANDS, source(-0.2187501).biomeAtQuart(quartX, 0, 0))
    }

    @Test
    fun erosionSamplesTheContainingChunkCenterAndBlockY() {
        val density = RecordingDensity()
        val source = TheEndBiomeSource(density)

        source.biomeAtQuart(-65 * 4 - 1, 17, 3)

        assertEquals(-1048, density.context?.blockX)
        assertEquals(68, density.context?.blockY)
        assertEquals(8, density.context?.blockZ)
    }

    private fun source(value: Double): TheEndBiomeSource = TheEndBiomeSource(ConstantDensity(value))

    private class ConstantDensity(
        private val value: Double,
    ) : DensityFunction {
        override fun compute(context: FunctionContext): Double = value

        override fun minValue(): Double = value

        override fun maxValue(): Double = value
    }

    private class RecordingDensity : DensityFunction {
        var context: FunctionContext? = null

        override fun compute(context: FunctionContext): Double {
            this.context = context
            return 0.0
        }

        override fun minValue(): Double = -1.0

        override fun maxValue(): Double = 1.0
    }
}
