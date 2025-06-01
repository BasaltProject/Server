package org.basaltmc.world.terrain

import org.basaltmc.registry.Biomes
import org.basaltmc.world.densityfunction.DensityFunction
import org.basaltmc.world.densityfunction.FunctionContext
import kotlin.test.Test
import kotlin.test.assertEquals

class NetherBiomeSourceTest {
    @Test
    fun exactPresetPointsSelectAllFiveBiomes() {
        assertEquals(Biomes.NETHER_WASTES, source(0.0, 0.0).biomeAt(0, 64, 0))
        assertEquals(Biomes.SOUL_SAND_VALLEY, source(0.0, -0.5).biomeAt(0, 64, 0))
        assertEquals(Biomes.CRIMSON_FOREST, source(0.4, 0.0).biomeAt(0, 64, 0))
        assertEquals(Biomes.WARPED_FOREST, source(0.0, 0.5).biomeAt(0, 64, 0))
        assertEquals(Biomes.BASALT_DELTAS, source(-0.5, 0.0).biomeAt(0, 64, 0))
    }

    @Test
    fun nonZeroOffsetsBiasTheOuterPresetsLikeVanilla() {
        assertEquals(Biomes.NETHER_WASTES, source(0.0, 0.39).biomeAt(0, 0, 0))
        assertEquals(Biomes.WARPED_FOREST, source(0.0, 0.40).biomeAt(0, 0, 0))
        assertEquals(Biomes.NETHER_WASTES, source(-0.28, 0.0).biomeAt(0, 0, 0))
        assertEquals(Biomes.BASALT_DELTAS, source(-0.29, 0.0).biomeAt(0, 0, 0))
    }

    private fun source(
        temperature: Double,
        vegetation: Double,
    ): NetherBiomeSource = NetherBiomeSource(ConstantDensity(temperature), ConstantDensity(vegetation))

    private class ConstantDensity(
        private val value: Double,
    ) : DensityFunction {
        override fun compute(context: FunctionContext): Double = value

        override fun minValue(): Double = value

        override fun maxValue(): Double = value
    }
}
