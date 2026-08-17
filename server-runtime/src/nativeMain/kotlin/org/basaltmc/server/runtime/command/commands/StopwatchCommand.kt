package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.DoubleArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.ResourceLocationArgument
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component

/**
 * Named stopwatches, kept in memory and read back by id.
 *
 * parity: `Stopwatches`, which counts wall-clock milliseconds rather than ticks, so a frozen or
 * lagging server does not change what one reads.
 */
public class Stopwatches(
    private val nowMillis: () -> Long,
) {
    private val started = mutableMapOf<Identifier, Long>()

    public fun ids(): List<Identifier> = started.keys.toList()

    public fun create(id: Identifier): Boolean {
        if (id in started) return false
        started[id] = nowMillis()
        return true
    }

    public fun restart(id: Identifier): Boolean {
        if (id !in started) return false
        started[id] = nowMillis()
        return true
    }

    public fun remove(id: Identifier): Boolean = started.remove(id) != null

    /** Seconds since it was created or last restarted, or null for one nobody made. */
    public fun elapsedSeconds(id: Identifier): Double? {
        val start = started[id] ?: return null
        return (nowMillis() - start) / MILLIS_PER_SECOND
    }

    private companion object {
        const val MILLIS_PER_SECOND = 1000.0
    }
}

/** parity: `StopwatchCommand`, new in 26.2. */
public class StopwatchCommand(
    private val stopwatches: Stopwatches,
) : Command(name = "stopwatch", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("create") {
            argument("id", ResourceLocationArgument) { executes { context -> create(context) } }
        }
        root.literal("query") {
            argument("id", ResourceLocationArgument) {
                suggestsFromServer { context -> context.matchingIds(stopwatches.ids()) }
                executes { context -> query(context, 1.0) }
                argument("scale", DoubleArgument()) { executes { context -> query(context, context.get("scale")) } }
            }
        }
        root.literal("restart") {
            argument("id", ResourceLocationArgument) {
                suggestsFromServer { context -> context.matchingIds(stopwatches.ids()) }
                executes { context -> restart(context) }
            }
        }
        root.literal("remove") {
            argument("id", ResourceLocationArgument) {
                suggestsFromServer { context -> context.matchingIds(stopwatches.ids()) }
                executes { context -> remove(context) }
            }
        }
    }

    private fun create(context: CommandContext): Int {
        val id = context.get<Identifier>("id")
        if (!stopwatches.create(id)) {
            throw CommandSyntaxException(
                Component.translatable("commands.stopwatch.already_exists", Component.text(id.toString())),
            )
        }
        context.success(
            Component.translatable("commands.stopwatch.create.success", Component.text(id.toString())),
            broadcastToAdmins = true,
        )
        return 1
    }

    /** parity: reports the seconds and returns them scaled, which is what `/execute store` reads. */
    private fun query(
        context: CommandContext,
        scale: Double,
    ): Int {
        val id = context.get<Identifier>("id")
        val seconds = stopwatches.elapsedSeconds(id) ?: throw missing(id)
        context.success(
            Component.translatable(
                "commands.stopwatch.query",
                Component.text(id.toString()),
                Component.text(seconds.toString()),
            ),
        )
        return (seconds * scale).toInt()
    }

    private fun restart(context: CommandContext): Int {
        val id = context.get<Identifier>("id")
        if (!stopwatches.restart(id)) throw missing(id)
        context.success(
            Component.translatable("commands.stopwatch.restart.success", Component.text(id.toString())),
            broadcastToAdmins = true,
        )
        return 1
    }

    private fun remove(context: CommandContext): Int {
        val id = context.get<Identifier>("id")
        if (!stopwatches.remove(id)) throw missing(id)
        context.success(
            Component.translatable("commands.stopwatch.remove.success", Component.text(id.toString())),
            broadcastToAdmins = true,
        )
        return 1
    }

    private fun missing(id: Identifier) =
        CommandSyntaxException(
            Component.translatable("commands.stopwatch.does_not_exist", Component.text(id.toString())),
        )
}
