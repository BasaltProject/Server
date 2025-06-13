package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.FloatArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.TimeArgument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.WorldClock
import org.basaltmc.wire.text.Component

/**
 * parity: `TimeCommand` as 26.2 rewrote it: clocks, not "day time".
 *
 * `set day`/`noon`/`night`/`midnight` are markers on the `minecraft:day` timeline: they move the
 * clock to the next occurrence rather than to an absolute number. A dimension with no clock (the
 * Nether) refuses by name, as vanilla does.
 *
 * `/time of <clock>` is not declared: each world here owns its clock (see [LoadedWorld.clock]).
 */
public class TimeCommand : Command(name = "time", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("set") {
            argument("time", TimeArgument(min = 0)) {
                executes { context -> setAbsolute(context, context.get<Int>("time").toLong()) }
            }
            for (marker in WorldClock.TIME_MARKERS.keys) {
                literal(marker) { executes { context -> setMarker(context, marker) } }
            }
        }
        root.literal("add") {
            // parity: `TimeArgument.time(Integer.MIN_VALUE)`. Only `add` accepts a negative, and it
            // then clamps at zero rather than running the clock backwards.
            argument("time", TimeArgument(min = Int.MIN_VALUE)) {
                executes { context -> addTime(context, context.get("time")) }
            }
        }
        root.literal("pause") { executes { context -> setPaused(context, true) } }
        root.literal("resume") { executes { context -> setPaused(context, false) } }
        root.literal("rate") {
            argument("rate", FloatArgument(min = MIN_RATE, max = MAX_RATE)) {
                executes { context -> setRate(context, context.get("rate")) }
            }
        }
        root.literal("query") {
            literal("time") { executes { context -> queryTime(context) } }
            literal("gametime") { executes { context -> queryGameTime(context) } }
            literal("day") { executes { context -> queryDay(context) } }
        }
    }

    private fun clockOf(context: CommandContext): Pair<LoadedWorld, String> {
        val world = context.world()
        val clockName =
            when (world.declaration.dimension) {
                WorldDimension.OVERWORLD -> "minecraft:overworld"
                WorldDimension.END -> "minecraft:the_end"
                WorldDimension.NETHER ->
                    throw CommandSyntaxException(
                        Component.translatable(
                            "commands.time.no_default_clock",
                            Component.text(world.declaration.dimension.registryKey),
                        ),
                    )
            }
        return world to clockName
    }

    private fun setAbsolute(
        context: CommandContext,
        ticks: Long,
    ): Int {
        val (world, clockName) = clockOf(context)
        world.clock.setTotalTicks(ticks)
        context.environment.syncTime(world)
        context.success(
            Component.translatable(
                "commands.time.set.absolute",
                Component.text(clockName),
                Component.text(ticks.toString()),
            ),
            broadcastToAdmins = true,
        )
        return wrap(ticks)
    }

    private fun setMarker(
        context: CommandContext,
        marker: String,
    ): Int {
        val (world, clockName) = clockOf(context)
        val ticks = WorldClock.TIME_MARKERS.getValue(marker)
        world.clock.setTotalTicks(WorldClock.timeOfNextMarker(world.clock.totalTicks, ticks))
        context.environment.syncTime(world)
        context.success(
            Component.translatable(
                "commands.time.set.time_marker",
                Component.text(clockName),
                Component.text("minecraft:$marker"),
            ),
            broadcastToAdmins = true,
        )
        return wrap(world.clock.totalTicks)
    }

    private fun addTime(
        context: CommandContext,
        ticks: Int,
    ): Int {
        val (world, clockName) = clockOf(context)
        world.clock.addTicks(ticks)
        context.environment.syncTime(world)
        context.success(
            Component.translatable(
                "commands.time.set.absolute",
                Component.text(clockName),
                Component.text(world.clock.totalTicks.toString()),
            ),
            broadcastToAdmins = true,
        )
        return wrap(world.clock.totalTicks)
    }

    private fun setPaused(
        context: CommandContext,
        paused: Boolean,
    ): Int {
        val (world, clockName) = clockOf(context)
        world.clock.paused = paused
        context.environment.syncTime(world)
        context.success(
            Component.translatable(
                if (paused) "commands.time.pause" else "commands.time.resume",
                Component.text(clockName),
            ),
            broadcastToAdmins = true,
        )
        return 1
    }

    private fun setRate(
        context: CommandContext,
        rate: Float,
    ): Int {
        val (world, clockName) = clockOf(context)
        world.clock.rate = rate
        context.environment.syncTime(world)
        context.success(
            Component.translatable("commands.time.rate", Component.text(clockName), Component.text(rate.toString())),
            broadcastToAdmins = true,
        )
        return 1
    }

    private fun queryTime(context: CommandContext): Int {
        val (world, clockName) = clockOf(context)
        val ticks = world.clock.totalTicks
        context.success(
            Component.translatable(
                "commands.time.query.absolute",
                Component.text(clockName),
                Component.text(ticks.toString()),
            ),
        )
        return wrap(ticks)
    }

    private fun queryGameTime(context: CommandContext): Int {
        val gameTime = context.world().gameTime
        context.success(
            Component.translatable("commands.time.query.gametime", Component.text(gameTime.toString())),
        )
        return wrap(gameTime)
    }

    /** Not vanilla's: kept from the old `/time query day`, which operators still reach for. */
    private fun queryDay(context: CommandContext): Int {
        val days = context.world().clock.totalTicks / WorldClock.DAY_LENGTH
        context.success(Component.text("Day $days"))
        return wrap(days)
    }

    /** parity: `TimeCommand.wrapTime`, which is what makes a huge tick count still fit a result. */
    private fun wrap(ticks: Long): Int = (ticks % Int.MAX_VALUE).toInt()

    private companion object {
        const val MIN_RATE = 1.0E-5f
        const val MAX_RATE = 1000f
    }
}
