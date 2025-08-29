package org.basaltmc.server.world

import org.basaltmc.server.world.densityfunction.MarkerType
import kotlin.time.TimeSource

/**
 * Opt-in counters and per-phase timings for chunk generation, off by default and read through
 * `[performance] generation-metrics` in `basalt.toml`.
 *
 * Deliberately not thread safe: enabling it also forces single-threaded generation (see
 * `BasaltServer`'s own startup path), because an atomic increment per noise evaluation would cost
 * more than several of the operations being counted and would distort the very numbers this
 * exists to produce. Call sites guard on [enabled] themselves rather than letting these methods
 * check, so a disabled build pays one predicted branch instead of a call.
 */
public object GenerationMetrics {
    /** One measured step of [org.basaltmc.server.world.terrain.RealTerrainGenerator.generate]. */
    public enum class Phase {
        TERRAIN_FILL,
        SURFACE_RULES,
        CARVERS,
        STRUCTURES,
        DECORATION,
        SURFACE_FREEZE,
        BIOME_FILL,

        /**
         * Undecorated neighbours generated so a feature could read past the chunk edge. Nested
         * inside DECORATION, and its own work is also counted in the phases above it, so this is
         * how much of those belongs to a neighbour rather than to the chunk being generated.
         */
        NEIGHBOURS,
    }

    public var enabled: Boolean = false

    private val phaseNanos = LongArray(Phase.entries.size)
    private val markerHits = LongArray(MarkerType.entries.size)
    private var chunks = 0L
    private var normalNoiseEvaluations = 0L
    private var biomeLookups = 0L
    private var biomeCandidatesScanned = 0L
    private var heightmapScans = 0L
    private var heightmapScanSteps = 0L

    public fun countChunk() {
        chunks++
    }

    public fun countNormalNoise() {
        normalNoiseEvaluations++
    }

    /** One top-down column scan for a heightmap value, the unit of work Chunk.heightAt does. */
    public fun countHeightmapScan(steps: Int) {
        heightmapScans++
        heightmapScanSteps += steps
    }

    public fun countBiomeLookup() {
        biomeLookups++
    }

    /** One climate-region distance computation, whether from the index or a full scan. */
    public fun countClimateComparison() {
        biomeCandidatesScanned++
    }

    public fun countMarker(type: MarkerType) {
        markerHits[type.ordinal]++
    }

    public fun addPhaseNanos(
        phase: Phase,
        nanos: Long,
    ) {
        phaseNanos[phase.ordinal] += nanos
    }

    public inline fun <T> measure(
        phase: Phase,
        block: () -> T,
    ): T {
        if (!enabled) return block()
        val start = TimeSource.Monotonic.markNow()
        try {
            return block()
        } finally {
            addPhaseNanos(phase, start.elapsedNow().inWholeNanoseconds)
        }
    }

    public fun reset() {
        phaseNanos.fill(0L)
        markerHits.fill(0L)
        chunks = 0L
        normalNoiseEvaluations = 0L
        biomeLookups = 0L
        biomeCandidatesScanned = 0L
        heightmapScans = 0L
        heightmapScanSteps = 0L
    }

    public fun report(): String {
        if (chunks == 0L) return "No chunks generated, nothing to report."
        val lines = mutableListOf("Generation metrics over $chunks chunk(s), per chunk:")
        for (phase in Phase.entries) {
            val millis = phaseNanos[phase.ordinal] / 1_000_000.0 / chunks
            lines += "  ${phase.name.padEnd(14)} ${format(millis)} ms"
        }
        lines += "  NormalNoise evaluations  ${normalNoiseEvaluations / chunks}"
        lines += "  Biome lookups            ${biomeLookups / chunks}"
        lines += "  Climate comparisons      ${biomeCandidatesScanned / chunks}"
        lines += "  Heightmap column scans   ${heightmapScans / chunks}"
        lines += "  Heightmap scan steps     ${heightmapScanSteps / chunks}"
        lines += "  Cache markers reached:"
        for (type in MarkerType.entries) {
            val hits = markerHits[type.ordinal]
            if (hits != 0L) lines += "    ${type.name.padEnd(18)} ${hits / chunks}"
        }
        return lines.joinToString("\n")
    }

    private fun format(value: Double): String {
        val scaled = (value * 1000.0).toLong()
        return "${scaled / 1000}.${(scaled % 1000).toString().padStart(3, '0')}"
    }
}
