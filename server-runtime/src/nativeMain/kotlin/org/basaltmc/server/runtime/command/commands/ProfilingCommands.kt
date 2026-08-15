package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.GreedyStringArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.timings.TimingsReport
import org.basaltmc.wire.common.timings.Timings
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.TextColor

/**
 * parity: `DebugCommand`, whose `start`/`stop` bracket a profiling run and write a report.
 *
 * Vanilla's report is its own `ProfileResults` dump; this writes the same report `/timings report`
 * writes, because that is what this server measures with. `debug function` is not declared: it
 * traces a function's every command into a file, which needs a tracing hook the runner does not
 * have.
 */
public class DebugCommand(
    private val timings: Timings,
    private val report: TimingsReport,
    private val nowNanos: () -> Long,
    private val ticksRun: () -> Long,
) : Command(name = "debug", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("start") { executes { context -> start(context) } }
        root.literal("stop") { executes { context -> stop(context) } }
    }

    private var ticksAtStart = 0L

    /** Two decimals, which is what vanilla's own report prints. */
    private fun formatted(value: Double): String {
        val scaled = kotlin.math.round(value * 100.0).toLong()
        return "${scaled / 100}.${(scaled % 100).toString().padStart(2, '0')}"
    }

    private fun start(context: CommandContext): Int {
        if (timings.enabled) throw CommandSyntaxException(Component.translatable("commands.debug.alreadyRunning"))
        timings.reset(nowNanos())
        ticksAtStart = ticksRun()
        timings.enabled = true
        context.success(Component.translatable("commands.debug.started"), broadcastToAdmins = true)
        return 1
    }

    private fun stop(context: CommandContext): Int {
        if (!timings.enabled) throw CommandSyntaxException(Component.translatable("commands.debug.notRunning"))
        timings.enabled = false
        val path = report.write(nowNanos = nowNanos())
        val seconds = timings.windowNanos(nowNanos()) / 1_000_000_000.0
        val ticks = ticksRun() - ticksAtStart
        context.success(
            Component.translatable(
                "commands.debug.stopped",
                Component.text(formatted(seconds)),
                Component.text(ticks.toString()),
                Component.text(formatted(if (seconds <= 0.0) 0.0 else ticks / seconds)),
            ),
            broadcastToAdmins = true,
        )
        context.source.sendSystemMessage(Component.text("  $path", TextColor.GRAY))
        return 1
    }
}

/**
 * parity: `PerfCommand`, which in vanilla records a ten-second profile for a bug report. The same
 * measurements live behind `/timings` here, so this starts and stops that and says where the file
 * went rather than inventing a second profiler.
 */
public class PerfCommand(
    private val timings: Timings,
    private val report: TimingsReport,
    private val nowNanos: () -> Long,
) : Command(name = "perf", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("start") {
            executes { context ->
                if (timings.enabled) {
                    throw CommandSyntaxException(Component.translatable("commands.perf.alreadyRunning"))
                }
                timings.reset(nowNanos())
                timings.enabled = true
                context.success(Component.translatable("commands.perf.started"), broadcastToAdmins = true)
                1
            }
        }
        root.literal("stop") {
            executes { context ->
                if (!timings.enabled) throw CommandSyntaxException(Component.translatable("commands.perf.notRunning"))
                timings.enabled = false
                val path = report.write(nowNanos = nowNanos())
                context.success(
                    Component.translatable("commands.perf.reportSaved", Component.text(path)),
                    broadcastToAdmins = true,
                )
                context.source.sendSystemMessage(Component.text("  $path", TextColor.GRAY))
                1
            }
        }
    }
}

/**
 * `/jfr`: kept so the command exists where an operator expects it, and answers instead of failing.
 *
 * Java Flight Recorder is a JVM facility and this server does not run on one, which is the point of
 * the project rather than a gap in it. The command reports that and points at `/timings`, which
 * measures the same things. It stays registered so that whoever does implement a native recorder
 * has the command, its permission and its tree already in place.
 */
public class JfrCommand(
    private val timings: Timings,
) : Command(name = "jfr", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        for (action in listOf("start", "stop")) {
            root.literal(action) { executes { context -> explain(context) } }
        }
    }

    private fun explain(context: CommandContext): Int {
        context.success(
            Component.text(
                "This server does not run on a JVM, so there is no Java Flight Recorder to $RECORDING_VERB.",
            ),
        )
        context.source.sendSystemMessage(
            Component.text(
                "  Use /timings for the same measurements" +
                    if (timings.enabled) " (already recording)." else ", or /perf for a one-off report.",
                TextColor.GRAY,
            ),
        )
        // Zero rather than one: nothing was recorded, and `/execute store` should see that.
        return 0
    }

    private companion object {
        const val RECORDING_VERB = "start or stop"
    }
}

/**
 * `/publish`: vanilla registers this only on the *integrated* server, where it opens a
 * single-player world to the local network. A dedicated server is already published, on the port
 * `server.toml` names, so the command answers with that rather than pretending to open a second one.
 */
public class PublishCommand(
    private val port: () -> Int,
) : Command(name = "publish", permissionLevel = PermissionLevel.OWNERS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context ->
            context.success(
                Component.text(
                    "This is a dedicated server: it is already accepting connections on port ${port()}. " +
                        "/publish exists only on a single-player world being opened to a local network.",
                ),
            )
            port()
        }
    }
}

/**
 * parity: `FetchProfileCommand`, which asks Mojang for the UUID behind a name.
 *
 * Only the `name` form: its `id` and `entity` forms answer from the server's profile cache, which
 * this build has not got, so they are left out rather than answered wrong.
 */
public class FetchProfileCommand(
    private val lookup: (String) -> String?,
) : Command(name = "fetchprofile", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("name") {
            argument("name", GreedyStringArgument) {
                executes { context ->
                    val name = context.get<String>("name")
                    val uuid =
                        lookup(name)
                            ?: throw CommandSyntaxException(
                                Component.text("No Mojang account is called '$name', or the lookup failed."),
                            )
                    context.success(Component.text("$name is $uuid"))
                    1
                }
            }
        }
    }
}
