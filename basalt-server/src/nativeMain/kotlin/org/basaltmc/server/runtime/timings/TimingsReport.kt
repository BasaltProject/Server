package org.basaltmc.server.runtime.timings

import kotlinx.datetime.TimeZone
import kotlinx.datetime.toLocalDateTime
import org.basaltmc.wire.common.timings.DurationSummary
import org.basaltmc.wire.common.timings.TickDurations
import org.basaltmc.wire.common.timings.Timings
import org.basaltmc.wire.io.FileIO
import kotlin.time.Clock
import kotlin.time.ExperimentalTime

/** What the report can ask the running server about itself, so this file needs nothing global. */
public class ReportSources(
    public val versionName: String,
    public val protocolVersion: Int,
    public val uptimeSeconds: () -> Long,
    public val onlinePlayers: () -> Int,
    public val maxPlayers: () -> Int,
    public val loadedChunks: () -> Int,
    public val worldCount: () -> Int,
    public val cpuPercent: () -> Double?,
    public val liveHeapBytes: () -> Long?,
)

/**
 * The `.json` a `/timings` run leaves behind.
 *
 * Written locally and never uploaded anywhere. spark posts its reports to a viewer, which is the
 * right call for a plugin that cannot ask an operator to read protobuf; a file on the server's own
 * disk owes nobody a round trip, and a performance report is exactly the sort of thing that
 * silently carries the world seed, player names and file paths off the machine.
 *
 * The shape is deliberately flat and self-describing: what a section cost, how often it ran, and the
 * distribution of tick durations behind it. Anything reading it can do so without a schema.
 */
@OptIn(ExperimentalTime::class)
public class TimingsReport(
    private val timings: Timings,
    private val tickDurations: TickDurations,
    private val sources: ReportSources,
) {
    /**
     * Writes the report and returns its path.
     *
     * The file is named after local time to the second, which is how an operator refers to a run
     * ("the one from just before it lagged") and what keeps two runs a minute apart from colliding.
     */
    public fun write(
        directory: String = DEFAULT_DIRECTORY,
        nowNanos: Long,
    ): String {
        FileIO.createDirectories(directory)
        val path = "$directory/${stamp()}.json"
        FileIO.writeText(path, json(nowNanos))
        return path
    }

    public fun json(nowNanos: Long): String {
        val builder = StringBuilder()
        builder.append("{\n")
        builder.append("""  "basalt": ${quote(sources.versionName)},""").append('\n')
        builder.append("""  "protocol": ${sources.protocolVersion},""").append('\n')
        builder.append("""  "generated": ${quote(stamp())},""").append('\n')
        builder.append("""  "uptime_seconds": ${sources.uptimeSeconds()},""").append('\n')
        builder.append("""  "measuring": ${timings.enabled},""").append('\n')
        builder.append("""  "measured_seconds": ${timings.windowNanos(nowNanos) / TickDurations.NANOS_PER_SECOND},""")
        builder.append('\n')

        builder.append("""  "server": {""").append('\n')
        builder.append("""    "players_online": ${sources.onlinePlayers()},""").append('\n')
        builder.append("""    "players_max": ${sources.maxPlayers()},""").append('\n')
        builder.append("""    "worlds": ${sources.worldCount()},""").append('\n')
        builder.append("""    "chunks_loaded": ${sources.loadedChunks()},""").append('\n')
        builder.append("""    "cpu_percent_of_one_core": ${number(sources.cpuPercent())},""").append('\n')
        builder.append("""    "heap_bytes_live": ${sources.liveHeapBytes() ?: "null"}""").append('\n')
        builder.append("  },\n")

        builder.append("""  "ticks": {""").append('\n')
        builder.append("""    "target_per_second": 20,""").append('\n')
        builder.append("""    "per_second": {""").append('\n')
        val rates = listOf(5, 10, 60, 300, 900)
        rates.forEachIndexed { index, window ->
            val comma = if (index == rates.lastIndex) "" else ","
            builder.append(
                """      "${label(window)}": ${number(tickDurations.ticksPerSecond(nowNanos, window))}$comma""",
            )
            builder.append('\n')
        }
        builder.append("    },\n")
        builder.append("""    "duration_millis": {""").append('\n')
        val windows = listOf(10, 60, 300)
        windows.forEachIndexed { index, window ->
            val comma = if (index == windows.lastIndex) "" else ","
            builder.append("""      "${label(window)}": ${summary(tickDurations.summary(nowNanos, window))}$comma""")
            builder.append('\n')
        }
        builder.append("    }\n")
        builder.append("  },\n")

        val sections = timings.snapshot().filter { it.calls > 0 }
        builder.append("""  "sections": [""").append('\n')
        sections.forEachIndexed { index, section ->
            val comma = if (index == sections.lastIndex) "" else ","
            builder.append("    {")
            builder.append(""""name": ${quote(section.name)}, """)
            builder.append(""""calls": ${section.calls}, """)
            builder.append(""""total_millis": ${number(section.totalNanos / 1_000_000.0)}, """)
            builder.append(""""average_millis": ${number(section.averageNanos / 1_000_000.0)}, """)
            builder.append(""""max_millis": ${number(section.maxNanos / 1_000_000.0)}""")
            builder.append("}$comma\n")
        }
        builder.append("  ]\n")
        builder.append("}\n")
        return builder.toString()
    }

    private fun summary(value: DurationSummary): String =
        """{"samples": ${value.samples}, "min": ${number(value.minMillis)}, """ +
            """"median": ${number(value.medianMillis)}, "p95": ${number(value.percentile95Millis)}, """ +
            """"max": ${number(value.maxMillis)}, "mean": ${number(value.meanMillis)}}"""

    private fun label(seconds: Int): String =
        when {
            seconds < 60 -> "${seconds}s"
            seconds < 3600 -> "${seconds / 60}m"
            else -> "${seconds / 3600}h"
        }

    /**
     * Three decimals, and `null` for what the platform could not answer rather than a made-up zero.
     *
     * Three because most of what is measured here takes well under a millisecond, and two rendered
     * every one of them as `0.00`.
     */
    private fun number(value: Double?): String {
        if (value == null) return "null"
        val rounded = kotlin.math.round(value * 1000).toLong()
        return "${rounded / 1000}.${(rounded % 1000).toString().padStart(3, '0')}"
    }

    private fun quote(value: String): String = "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"") + "\""

    private fun stamp(): String {
        val now = Clock.System.now().toLocalDateTime(TimeZone.currentSystemDefault())

        fun pad(value: Int) = value.toString().padStart(2, '0')
        return "${now.year}-${pad(now.month.ordinal + 1)}-${pad(now.day)}_" +
            "${pad(now.hour)}-${pad(now.minute)}-${pad(now.second)}"
    }

    public companion object {
        /** Beside the logs and the asset cache, in the directory the server already owns. */
        public const val DEFAULT_DIRECTORY: String = "./.basalt/timings"
    }
}
