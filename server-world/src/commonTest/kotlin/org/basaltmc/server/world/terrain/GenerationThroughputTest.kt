package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.GenerationMetrics
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertTrue
import kotlin.time.ExperimentalTime
import kotlin.time.TimeSource

/**
 * Wall time for a chunk, with the phase breakdown that says where it went. Vanilla generates one in
 * single-digit milliseconds and this fork exists to be faster, so the ceiling is a ratchet: lower it
 * whenever a change earns it, never raise it to make a regression pass.
 */
@OptIn(ExperimentalTime::class)
class GenerationThroughputTest {
    private val world = SyntheticOverworld()

    @AfterTest
    fun tearDown() {
        GenerationMetrics.enabled = false
        GenerationMetrics.reset()
    }

    @Test
    fun aChunkCostsWhatTheBreakdownSays() {
        // Warm up, so the first chunk's lazily built tables are not billed to the measurement.
        RealTerrainGenerator.generate(world.router, world.environment, -1, -1)

        GenerationMetrics.reset()
        GenerationMetrics.enabled = true
        val started = TimeSource.Monotonic.markNow()
        for (chunkX in 0 until CHUNKS) RealTerrainGenerator.generate(world.router, world.environment, chunkX, 0)
        val elapsed = started.elapsedNow()
        GenerationMetrics.enabled = false

        val perChunk = elapsed.inWholeMicroseconds / 1000.0 / CHUNKS
        println("Wall time: $perChunk ms/chunk over $CHUNKS chunks")
        println(GenerationMetrics.report())

        assertTrue(perChunk < CEILING_MILLIS, "a chunk took $perChunk ms, over the $CEILING_MILLIS ms ceiling")
    }

    private companion object {
        const val CHUNKS = 8

        // Deliberately loose: a debug binary on a loaded runner is not the number being defended,
        // and the deterministic guards live in GenerationCostTest. This catches a collapse.
        const val CEILING_MILLIS = 1_200.0
    }
}
