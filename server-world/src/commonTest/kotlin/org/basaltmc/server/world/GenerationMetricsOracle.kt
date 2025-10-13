package org.basaltmc.server.world

import org.basaltmc.server.world.densityfunction.MarkerType
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the metrics report, which is the only observable it has. */
class GenerationMetricsOracle {
    @Test
    fun dump() {
        GenerationMetrics.reset()
        println("ORACLE empty text=${GenerationMetrics.report().replace("\n", "|")}")

        GenerationMetrics.enabled = true
        repeat(4) { GenerationMetrics.countChunk() }
        repeat(9) { GenerationMetrics.countNormalNoise() }
        repeat(3) { GenerationMetrics.countBiomeLookup() }
        repeat(17) { GenerationMetrics.countClimateComparison() }
        GenerationMetrics.countHeightmapScan(11)
        GenerationMetrics.countHeightmapScan(5)
        GenerationMetrics.countMarker(MarkerType.FLAT_CACHE)
        GenerationMetrics.countMarker(MarkerType.FLAT_CACHE)
        GenerationMetrics.countMarker(MarkerType.INTERPOLATED)
        // Nanoseconds fed straight in, so the report is exact rather than a timing.
        GenerationMetrics.addPhaseNanos(GenerationMetrics.Phase.TERRAIN_FILL, 12_345_678L)
        GenerationMetrics.addPhaseNanos(GenerationMetrics.Phase.DECORATION, 4_000_000L)
        GenerationMetrics.addPhaseNanos(GenerationMetrics.Phase.NEIGHBOURS, 999L)
        println("ORACLE filled text=${GenerationMetrics.report().replace("\n", "|")}")

        var ran = 0
        GenerationMetrics.measure(GenerationMetrics.Phase.CARVERS) { ran++ }
        GenerationMetrics.enabled = false
        GenerationMetrics.measure(GenerationMetrics.Phase.CARVERS) { ran++ }
        println("ORACLE measure ran=$ran")

        GenerationMetrics.reset()
        println("ORACLE afterReset text=${GenerationMetrics.report().replace("\n", "|")}")
    }
}
