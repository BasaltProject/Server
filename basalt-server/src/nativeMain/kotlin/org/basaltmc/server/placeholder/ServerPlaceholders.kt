package org.basaltmc.server.placeholder

import kotlinx.datetime.TimeZone
import kotlinx.datetime.toLocalDateTime
import org.basaltmc.common.placeholder.PlaceholderCatalog
import org.basaltmc.common.placeholder.PlaceholderContext
import org.basaltmc.common.placeholder.PlaceholderProvider
import org.basaltmc.common.placeholder.PlaceholderValue
import kotlin.time.Clock
import kotlin.time.ExperimentalTime
import kotlin.time.Instant

/** What the server can be asked about itself, read live rather than captured once. */
public class ServerFacts(
    public val versionName: String,
    public val protocolVersion: Int,
    public val startedAt: Instant,
    public val maxPlayers: () -> Int,
    /** Names of the players connected, by world, which answers both the count and the per-world count. */
    public val onlineByWorld: () -> Map<String, Int>,
    public val uniqueJoins: () -> Long,
    public val loadedChunks: () -> Int,
    public val liveHeapBytes: () -> Long?,
)

/**
 * `%basalt_server_*%`.
 *
 * The ones this build cannot answer are registered all the same, with the reason: a server list that
 * asks for the TPS should be told that nothing measures it yet, not that it invented a placeholder.
 */
@OptIn(ExperimentalTime::class)
public class ServerPlaceholders(
    private val facts: ServerFacts,
) : PlaceholderProvider,
    PlaceholderCatalog {
    private val answers: Map<String, () -> PlaceholderValue> =
        mapOf(
            "version" to { PlaceholderValue.of(facts.versionName) },
            "protocol" to { PlaceholderValue.of(facts.protocolVersion) },
            "online" to { PlaceholderValue.of(facts.onlineByWorld().values.sum()) },
            "max_players" to { PlaceholderValue.of(facts.maxPlayers()) },
            "unique_joins" to { PlaceholderValue.of(facts.uniqueJoins()) },
            "total_chunks" to { PlaceholderValue.of(facts.loadedChunks()) },
            "uptime" to { PlaceholderValue.of(formatUptime(Clock.System.now() - facts.startedAt)) },
            "uptime_seconds" to { PlaceholderValue.of((Clock.System.now() - facts.startedAt).inWholeSeconds) },
            // The one memory figure a native runtime with no fixed heap can honestly report.
            "ram_used" to { PlaceholderValue.of(facts.liveHeapBytes()?.let { it / 1_048_576 }) },
        )

    override fun names(): Collection<String> =
        (answers.keys + NOT_IMPLEMENTED.keys + listOf("online_<world>", "time_<format>")).sorted()

    override fun resolve(
        params: String,
        context: PlaceholderContext,
    ): PlaceholderValue {
        NOT_IMPLEMENTED[params]?.let { return notImplemented(it) }
        if (params.startsWith("online_")) {
            val world = params.removePrefix("online_")
            return PlaceholderValue.of(facts.onlineByWorld()[world] ?: 0)
        }
        if (params.startsWith("countdown_")) {
            return notImplemented("counting down to a date is not implemented yet")
        }
        if (params.startsWith("time_")) {
            return PlaceholderValue.of(formatNow(params.removePrefix("time_")))
        }
        return answers[params]?.invoke() ?: PlaceholderValue.Unknown
    }

    private companion object {
        val NOT_IMPLEMENTED =
            buildMap {
                for (
                name in listOf(
                    "tps",
                    "tps_1",
                    "tps_5",
                    "tps_15",
                    "tps_1_colored",
                    "tps_5_colored",
                    "tps_15_colored",
                )
                ) {
                    // Measuring it would report the sleep, not the work: the loop sleeps a fixed 50ms
                    // per tick instead of subtracting how long the tick took, so it can only ever
                    // read just under 20. A real figure needs that loop fixed first.
                    put(name, "the tick loop does not measure or compensate for tick time yet")
                }
                for (name in listOf("ram_free", "ram_total", "ram_max")) {
                    put(name, "the native runtime has no fixed heap: only what is live after a collection is known")
                }
                put("name", "there is no server name setting yet")
                put("countdown", "counting down to a date is not implemented yet")
                put("has_whitelist", "there is no whitelist yet")
                put("total_entities", "there is no entity system yet")
                put("total_living_entities", "there is no entity system yet")
            }

        fun formatUptime(elapsed: kotlin.time.Duration): String {
            val days = elapsed.inWholeDays
            val hours = elapsed.inWholeHours % 24
            val minutes = elapsed.inWholeMinutes % 60
            val seconds = elapsed.inWholeSeconds % 60
            return buildString {
                if (days > 0) append("${days}d ")
                if (days > 0 || hours > 0) append("${hours}h ")
                if (days > 0 || hours > 0 || minutes > 0) append("${minutes}m ")
                append("${seconds}s")
            }
        }

        /**
         * The subset of `SimpleDateFormat` worth supporting: `yyyy MM dd HH mm ss`, anything else
         * literal.
         *
         * Not the whole thing, and deliberately: the rest of that format language is eras, week
         * numbers, time zones and locale-dependent month names, none of which a server list line
         * needs, and half of which would be wrong without a locale to render them in.
         */
        fun formatNow(pattern: String): String {
            val now = Clock.System.now().toLocalDateTime(TimeZone.currentSystemDefault())
            return pattern
                .replace("yyyy", now.year.toString().padStart(4, '0'))
                .replace("MM", (now.month.ordinal + 1).toString().padStart(2, '0'))
                .replace("dd", now.day.toString().padStart(2, '0'))
                .replace("HH", now.hour.toString().padStart(2, '0'))
                .replace("mm", now.minute.toString().padStart(2, '0'))
                .replace("ss", now.second.toString().padStart(2, '0'))
        }
    }
}
