package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.FloatArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.RegistryArgument
import org.basaltmc.server.runtime.command.ResourceLocationArgument
import org.basaltmc.server.runtime.command.TimeArgument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.WorldClock
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.MiniMessage

/**
 * parity: `TimeCommand` as 26.2 rewrote it: clocks, not "day time".
 *
 * `set day`/`noon`/`night`/`midnight` are markers on the `minecraft:day` timeline: they move the
 * clock to the next occurrence rather than to an absolute number. A dimension with no clock (the
 * Nether) refuses by name, as vanilla does.
 *
 * `/time of <clock>` names the clock explicitly, and here it has to be the one the source's own
 * world runs on: clocks belong to worlds in this build (see [LoadedWorld.clock]), not to a manager
 * shared by every dimension of a save.
 */
public class TimeCommand : Command(name = "time", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        actions(root)
        root.literal("of") {
            argument("clock", RegistryArgument.resource("minecraft:world_clock")) {
                suggestsFromServer { context -> context.matchingIds(CLOCKS) }
                actions(this, named = true)
            }
        }
    }

    /** The verbs, declared once for the bare form and once under `of <clock>`, as vanilla does. */
    private fun actions(
        root: CommandNodeBuilder,
        named: Boolean = false,
    ) {
        root.literal("set") {
            argument("time", TimeArgument(min = 0)) {
                executes { context -> setAbsolute(context, context.get<Int>("time").toLong(), named) }
            }
            argument("timemarker", ResourceLocationArgument) {
                suggestsFromServer { context ->
                    context.matchingIds(WorldClock.TIME_MARKERS.keys.map { Identifier.vanilla(it) })
                }
                executes { context -> setMarker(context, context.get("timemarker"), named) }
            }
        }
        root.literal("add") {
            // parity: `TimeArgument.time(Integer.MIN_VALUE)`. Only `add` accepts a negative, and it
            // then clamps at zero rather than running the clock backwards.
            argument("time", TimeArgument(min = Int.MIN_VALUE)) {
                executes { context -> addTime(context, context.get("time"), named) }
            }
        }
        root.literal("pause") { executes { context -> setPaused(context, true, named) } }
        root.literal("resume") { executes { context -> setPaused(context, false, named) } }
        root.literal("rate") {
            argument("rate", FloatArgument(min = MIN_RATE, max = MAX_RATE)) {
                executes { context -> setRate(context, context.get("rate"), named) }
            }
        }
        root.literal("query") {
            literal("time") { executes { context -> queryTime(context, named) } }
            if (!named) {
                literal("gametime") { executes { context -> queryGameTime(context) } }
                literal("day") { executes { context -> queryDay(context) } }
            }
            argument("timeline", RegistryArgument.resource("minecraft:timeline")) {
                suggestsFromServer { context ->
                    context.matchingIds(
                        context.environment.timelines
                            ?.ids
                            .orEmpty(),
                    )
                }
                executes { context -> queryTimeline(context, named, repetitions = false) }
                literal("repetition") { executes { context -> queryTimeline(context, named, repetitions = true) } }
            }
        }
    }

    private fun clockOf(
        context: CommandContext,
        named: Boolean = false,
    ): Pair<LoadedWorld, String> {
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
        if (named) {
            val asked = context.get<Identifier>("clock").toString()
            if (asked != clockName) {
                throw CommandSyntaxException(
                    Component.text("This world runs on $clockName, not $asked: clocks belong to worlds here."),
                )
            }
        }
        return world to clockName
    }

    /**
     * parity: `time query <timeline>`, the position inside a timeline's period, and `repetition`,
     * how many whole periods have passed. A timeline that belongs to another clock is refused.
     */
    private fun queryTimeline(
        context: CommandContext,
        named: Boolean,
        repetitions: Boolean,
    ): Int {
        val (world, clockName) = clockOf(context, named)
        val id = context.get<Identifier>("timeline")
        val definition =
            context.environment.timelines?.definitionOf(id)
                ?: throw CommandSyntaxException(
                    Component.translatable("commands.time.query.timeline.unknown", Component.text(id.toString())),
                )
        val timelineClock = definition.getString("clock")
        if (timelineClock.isNotEmpty() && timelineClock != clockName) {
            throw CommandSyntaxException(
                Component.text("$id runs on $timelineClock, not on $clockName."),
            )
        }
        val period = definition.getInt("period_ticks").takeIf { it > 0 } ?: DAY_TICKS
        val ticks = world.clock.totalTicks
        val value = if (repetitions) ticks / period else ticks % period
        context.success(
            Component.translatable(
                if (repetitions) "commands.time.query.timeline.repetitions" else "commands.time.query.timeline",
                Component.text(id.toString()),
                Component.text(value.toString()),
            ),
        )
        return wrap(value)
    }

    private fun setAbsolute(
        context: CommandContext,
        ticks: Long,
        named: Boolean,
    ): Int {
        val (world, clockName) = clockOf(context, named)
        val skip =
            context.environment.pluginEvents.emit(PluginEventTypes.WORLD_TIME_SKIP) { event ->
                event.set(0, world.name)
                event.set(1, ticks)
                event.set(2, world.clock.totalTicks)
                event.set(3, "command")
            }
        if (skip?.cancelled == true) {
            skip.cancelMessage?.let { context.source.sendSystemMessage(MiniMessage.parse(it)) }
            return 0
        }
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
        marker: Identifier,
        named: Boolean,
    ): Int {
        val (world, clockName) = clockOf(context, named)
        val ticks =
            WorldClock.TIME_MARKERS[marker.path]
                ?: throw CommandSyntaxException(
                    Component.translatable(
                        "commands.time.set.time_marker.not_found",
                        Component.text(clockName),
                        Component.text(marker.toString()),
                    ),
                )
        world.clock.setTotalTicks(WorldClock.timeOfNextMarker(world.clock.totalTicks, ticks))
        context.environment.syncTime(world)
        context.success(
            Component.translatable(
                "commands.time.set.time_marker",
                Component.text(clockName),
                Component.text(marker.toString()),
            ),
            broadcastToAdmins = true,
        )
        return wrap(world.clock.totalTicks)
    }

    private fun addTime(
        context: CommandContext,
        ticks: Int,
        named: Boolean,
    ): Int {
        val (world, clockName) = clockOf(context, named)
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
        named: Boolean,
    ): Int {
        val (world, clockName) = clockOf(context, named)
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
        named: Boolean,
    ): Int {
        val (world, clockName) = clockOf(context, named)
        world.clock.rate = rate
        context.environment.syncTime(world)
        context.success(
            Component.translatable("commands.time.rate", Component.text(clockName), Component.text(rate.toString())),
            broadcastToAdmins = true,
        )
        return 1
    }

    private fun queryTime(
        context: CommandContext,
        named: Boolean,
    ): Int {
        val (world, clockName) = clockOf(context, named)
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
        val CLOCKS = listOf(Identifier.vanilla("overworld"), Identifier.vanilla("the_end"))
        const val DAY_TICKS = 24000
        const val MIN_RATE = 1.0E-5f
        const val MAX_RATE = 1000f
    }
}
