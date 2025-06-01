package org.basaltmc.server.command.commands

import org.basaltmc.common.timings.TickDurations
import org.basaltmc.common.timings.Timings
import org.basaltmc.server.command.Command
import org.basaltmc.server.command.CommandContext
import org.basaltmc.server.command.CommandNodeBuilder
import org.basaltmc.server.command.PermissionLevel
import org.basaltmc.server.timings.TimingsReport
import org.basaltmc.text.Component
import org.basaltmc.text.TextColor

/**
 * `/timings`: what the server is spending its time on.
 *
 * Modelled on spark rather than on Bukkit's own timings, and with one deliberate omission: spark's
 * heart is a sampling profiler that reads other threads' call stacks, and Kotlin/Native offers no
 * way to do that. What is here instead is the half that does not need it, and which is most of what
 * anyone actually reads: the distribution of tick durations, and named sections of the server
 * measured directly.
 *
 * The distribution matters more than the rate, which is why it is here at all. A server pinned at 20
 * TPS with one tick in fifty taking 80ms is stuttering, and its TPS reads perfect; min/median/95th/max
 * says so immediately.
 *
 * Measuring is off until `/timings start`, because a clock read per section per call is small but
 * not free, and a server nobody is investigating should not pay it.
 */
public class TimingsCommand(
    private val timings: Timings,
    private val tickDurations: TickDurations,
    private val report: TimingsReport,
    private val nowNanos: () -> Long,
) : Command(name = "timings", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context -> summarise(context) }

        root.literal("start") {
            executes { context ->
                timings.reset(nowNanos())
                timings.enabled = true
                context.success(
                    Component.text("Measuring started. Run /timings report when you have seen the problem."),
                    broadcastToAdmins = true,
                )
                1
            }
        }

        root.literal("stop") {
            executes { context ->
                timings.enabled = false
                context.success(Component.text("Measuring stopped. The numbers so far are kept."), false)
                1
            }
        }

        root.literal("reset") {
            executes { context ->
                timings.reset(nowNanos())
                tickDurations.reset()
                context.success(Component.text("Counters cleared."), false)
                1
            }
        }

        root.literal("report") {
            executes { context ->
                val path = report.write(nowNanos = nowNanos())
                context.success(Component.text("Timings written to $path"), broadcastToAdmins = true)
                if (timings.snapshot().none { it.calls > 0 }) {
                    // Otherwise the file's empty `sections` reads as "nothing costs anything".
                    context.source.sendSystemMessage(
                        Component.text(
                            "  Sections are empty: nothing measurable ran. /timings start, let the " +
                                "server do the thing you are investigating, then report.",
                            TextColor.GRAY,
                        ),
                    )
                }
                1
            }
        }
    }

    /** The one screen worth reading in chat; everything else goes in the file. */
    private fun summarise(context: CommandContext): Int {
        val now = nowNanos()
        val source = context.source
        source.sendSystemMessage(Component.text("Ticks per second:", TextColor.GRAY))
        source.sendSystemMessage(
            Component.text(
                "  5s ${rate(tickDurations.ticksPerSecond(now, 5))}   " +
                    "1m ${rate(tickDurations.ticksPerSecond(now, 60))}   " +
                    "5m ${rate(tickDurations.ticksPerSecond(now, 300))}",
                TextColor.WHITE,
            ),
        )
        source.sendSystemMessage(
            Component.text("Tick duration in ms (min/median/95%/max):", TextColor.GRAY),
        )
        for (window in listOf(10, 60, 300)) {
            val summary = tickDurations.summary(now, window)
            val label = if (window < 60) "${window}s" else "${window / 60}m"
            source.sendSystemMessage(
                Component.text(
                    "  ${label.padEnd(4)} ${millis(summary.minMillis)} / ${millis(summary.medianMillis)} / " +
                        "${millis(summary.percentile95Millis)} / ${millis(summary.maxMillis)}" +
                        "   (${summary.samples} ticks)",
                    TextColor.WHITE,
                ),
            )
        }
        source.sendSystemMessage(
            Component.text(
                if (timings.enabled) {
                    "Measuring sections. /timings report writes them to a file."
                } else {
                    "Sections are not being measured. /timings start to begin."
                },
                TextColor.GRAY,
            ),
        )
        return 1
    }

    private fun rate(value: Double): String = decimals(value, 2)

    /**
     * Three decimals for durations, because the work being measured is often under a millisecond and
     * two decimals rendered a real 0.004ms tick as `0.00`, which reads as "no data".
     */
    private fun millis(value: Double): String = decimals(value, 3)

    private fun decimals(
        value: Double,
        digits: Int,
    ): String {
        var scale = 1L
        repeat(digits) { scale *= 10 }
        val rounded = kotlin.math.round(value * scale).toLong()
        return "${rounded / scale}.${(rounded % scale).toString().padStart(digits, '0')}"
    }
}
