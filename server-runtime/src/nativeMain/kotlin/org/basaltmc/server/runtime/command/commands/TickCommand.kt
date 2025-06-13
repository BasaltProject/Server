package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.TickLoop
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.FloatArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.TimeArgument
import org.basaltmc.wire.common.timings.TickDurations
import org.basaltmc.wire.protocol.play.TickingStatePacket
import org.basaltmc.wire.protocol.play.TickingStepPacket
import org.basaltmc.wire.text.Component

/**
 * parity: `TickCommand`. `freeze` stops the world without stopping the server: connections, chunk
 * streaming and commands all keep working, which is what makes `/tick unfreeze` reachable.
 */
public class TickCommand(
    private val tickLoop: TickLoop,
    private val durations: TickDurations,
    /** The same monotonic clock the tick loop records with, so a window means the same thing. */
    private val nowNanos: () -> Long,
) : Command(name = "tick", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("query") { executes { context -> query(context) } }
        root.literal("rate") {
            argument("rate", FloatArgument(min = TickLoop.MIN_TICK_RATE, max = TickLoop.MAX_TICK_RATE)) {
                executes { context -> setRate(context) }
            }
        }
        root.literal("freeze") { executes { context -> freeze(context, true) } }
        root.literal("unfreeze") { executes { context -> freeze(context, false) } }
        root.literal("step") {
            executes { context -> step(context, 1) }
            literal("stop") { executes { context -> stopStepping(context) } }
            argument("time", TimeArgument(min = 1)) {
                executes { context -> step(context, context.get("time")) }
            }
        }
        root.literal("sprint") {
            literal("stop") { executes { context -> stopSprint(context) } }
            argument("time", TimeArgument(min = 1)) {
                executes { context -> sprint(context, context.get("time")) }
            }
        }
    }

    private fun query(context: CommandContext): Int {
        val averageMillis = durations.summary(nowNanos(), QUERY_WINDOW_SECONDS).meanMillis
        context.success(
            Component.translatable(
                "commands.tick.query.rate.running",
                Component.text(format(tickLoop.tickRate)),
                Component.text(format(averageMillis.toFloat())),
                Component.text(format(1000f / tickLoop.tickRate)),
            ),
        )
        context.success(
            Component.translatable(
                if (tickLoop.isSprinting) {
                    "commands.tick.status.sprinting"
                } else if (tickLoop.isFrozen) {
                    "commands.tick.status.frozen"
                } else {
                    "commands.tick.status.running"
                },
            ),
        )
        return tickLoop.tickRate.toInt()
    }

    private fun setRate(context: CommandContext): Int {
        val rate = context.get<Float>("rate")
        tickLoop.setTickRate(rate)
        announce(context)
        context.success(
            Component.translatable("commands.tick.rate.success", Component.text(format(rate))),
            broadcastToAdmins = true,
        )
        return rate.toInt()
    }

    private fun freeze(
        context: CommandContext,
        frozen: Boolean,
    ): Int {
        tickLoop.setFrozen(frozen)
        announce(context)
        context.success(
            Component.translatable(
                if (frozen) "commands.tick.status.frozen" else "commands.tick.status.running",
            ),
            broadcastToAdmins = true,
        )
        return 1
    }

    private fun step(
        context: CommandContext,
        ticks: Int,
    ): Int {
        if (!tickLoop.step(ticks)) {
            throw CommandSyntaxException(Component.translatable("commands.tick.step.fail"))
        }
        for (player in context.environment.players.snapshot()) {
            player.connection.sendPlayPacket(TickingStepPacket.ID) { TickingStepPacket(ticks).write(it) }
        }
        context.success(
            Component.translatable("commands.tick.step.success", Component.text(ticks.toString())),
            broadcastToAdmins = true,
        )
        return ticks
    }

    private fun stopStepping(context: CommandContext): Int {
        tickLoop.step(0)
        context.success(Component.translatable("commands.tick.step.stop.success"), broadcastToAdmins = true)
        return 1
    }

    private fun sprint(
        context: CommandContext,
        ticks: Int,
    ): Int {
        tickLoop.sprint(ticks)
        announce(context)
        context.success(Component.translatable("commands.tick.status.sprinting"), broadcastToAdmins = true)
        return ticks
    }

    private fun stopSprint(context: CommandContext): Int {
        if (!tickLoop.stopSprinting()) {
            throw CommandSyntaxException(Component.translatable("commands.tick.sprint.stop.fail"))
        }
        announce(context)
        context.success(Component.translatable("commands.tick.sprint.stop.success"), broadcastToAdmins = true)
        return 1
    }

    /** The client draws its own "frozen" overlay from this, so every change has to reach it. */
    private fun announce(context: CommandContext) {
        val packet = TickingStatePacket(tickLoop.tickRate, tickLoop.isFrozen)
        for (player in context.environment.players.snapshot()) {
            player.connection.sendPlayPacket(TickingStatePacket.ID) { packet.write(it) }
        }
    }

    private fun format(value: Float): String {
        val scaled = kotlin.math.round(value * 10).toLong()
        return "${scaled / 10}." + (kotlin.math.abs(scaled) % 10).toString()
    }

    private companion object {
        /** How far back `/tick query` averages, matching what `/timings` calls "recent". */
        const val QUERY_WINDOW_SECONDS = 60
    }
}
