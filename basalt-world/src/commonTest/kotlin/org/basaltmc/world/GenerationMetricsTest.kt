package org.basaltmc.world

import org.basaltmc.world.densityfunction.MarkerType
import kotlin.test.AfterTest
import kotlin.test.BeforeTest
import kotlin.test.Test
import kotlin.test.assertContains
import kotlin.test.assertEquals

class GenerationMetricsTest {
    @BeforeTest
    fun setUp() {
        GenerationMetrics.reset()
        GenerationMetrics.enabled = true
    }

    @AfterTest
    fun tearDown() {
        GenerationMetrics.enabled = false
        GenerationMetrics.reset()
    }

    @Test
    fun `reports per chunk averages`() {
        repeat(2) {
            repeat(10) { GenerationMetrics.countNormalNoise() }
            GenerationMetrics.countBiomeLookup()
            repeat(100) { GenerationMetrics.countClimateComparison() }
            GenerationMetrics.countMarker(MarkerType.FLAT_CACHE)
            GenerationMetrics.countChunk()
        }

        val report = GenerationMetrics.report()
        assertContains(report, "over 2 chunk(s)")
        assertContains(report, "NormalNoise evaluations  10")
        assertContains(report, "Biome lookups            1")
        assertContains(report, "Climate comparisons      100")
        assertContains(report, "FLAT_CACHE")
    }

    @Test
    fun `reports nothing without a generated chunk`() {
        assertEquals("No chunks generated, nothing to report.", GenerationMetrics.report())
    }

    @Test
    fun `measure runs the block and records time when disabled too`() {
        GenerationMetrics.enabled = false
        var ran = false
        GenerationMetrics.measure(GenerationMetrics.Phase.TERRAIN_FILL) { ran = true }
        assertEquals(true, ran)
    }

    @Test
    fun `reset clears every counter`() {
        GenerationMetrics.countNormalNoise()
        GenerationMetrics.countChunk()
        GenerationMetrics.reset()
        assertEquals("No chunks generated, nothing to report.", GenerationMetrics.report())
    }
}
