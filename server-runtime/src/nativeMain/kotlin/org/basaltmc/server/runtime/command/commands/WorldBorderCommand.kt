package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.Coordinates
import org.basaltmc.server.runtime.command.DoubleArgument
import org.basaltmc.server.runtime.command.FloatArgument
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.TimeArgument
import org.basaltmc.server.runtime.command.Vec2Argument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.WorldBorder
import org.basaltmc.wire.protocol.play.SetBorderCenterPacket
import org.basaltmc.wire.protocol.play.SetBorderLerpSizePacket
import org.basaltmc.wire.protocol.play.SetBorderSizePacket
import org.basaltmc.wire.protocol.play.SetBorderWarningDelayPacket
import org.basaltmc.wire.protocol.play.SetBorderWarningDistancePacket
import org.basaltmc.wire.text.Component
import kotlin.math.abs

/**
 * parity: `WorldBorderCommand`.
 *
 * `damage` is stored and reported but nothing is hurt by it yet: damage outside the border belongs
 * to the per-tick player pass, not here.
 */
public class WorldBorderCommand : Command(name = "worldborder", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("get") { executes { context -> get(context) } }
        root.literal("set") {
            argument("distance", DoubleArgument(min = -MAX_SIZE, max = MAX_SIZE)) {
                executes { context -> setSize(context, 0) }
                argument("time", TimeArgument(min = 0)) {
                    executes { context -> setSize(context, context.get("time")) }
                }
            }
        }
        root.literal("add") {
            argument("distance", DoubleArgument(min = -MAX_SIZE, max = MAX_SIZE)) {
                executes { context -> addSize(context, 0) }
                argument("time", TimeArgument(min = 0)) {
                    executes { context -> addSize(context, context.get("time")) }
                }
            }
        }
        root.literal("center") {
            argument("pos", Vec2Argument) { executes { context -> center(context) } }
        }
        root.literal("damage") {
            literal("amount") {
                argument("damagePerBlock", FloatArgument(min = 0f)) {
                    executes { context -> damageAmount(context) }
                }
            }
            literal("buffer") {
                argument("distance", FloatArgument(min = 0f)) { executes { context -> damageBuffer(context) } }
            }
        }
        root.literal("warning") {
            literal("distance") {
                argument("distance", IntegerArgument(min = 0)) { executes { context -> warningDistance(context) } }
            }
            literal("time") {
                argument("time", TimeArgument(min = 0)) { executes { context -> warningTime(context) } }
            }
        }
    }

    private fun borderOf(context: CommandContext): Pair<LoadedWorld, WorldBorder> {
        val world = context.world()
        return world to world.border
    }

    private fun get(context: CommandContext): Int {
        val (_, border) = borderOf(context)
        val size = border.targetSize
        context.success(
            Component.translatable("commands.worldborder.get", Component.text(size.toLong().toString())),
        )
        return size.toInt()
    }

    private fun setSize(
        context: CommandContext,
        timeTicks: Int,
    ): Int = applySize(context, context.get<Double>("distance"), timeTicks)

    private fun addSize(
        context: CommandContext,
        timeTicks: Int,
    ): Int {
        val (_, border) = borderOf(context)
        return applySize(context, border.targetSize + context.get<Double>("distance"), timeTicks)
    }

    private fun applySize(
        context: CommandContext,
        newSize: Double,
        timeTicks: Int,
    ): Int {
        val (world, border) = borderOf(context)
        if (newSize == border.targetSize) {
            throw CommandSyntaxException(Component.translatable("commands.worldborder.set.failed.nochange"))
        }
        if (newSize < MIN_SIZE) {
            throw CommandSyntaxException(Component.translatable("commands.worldborder.set.failed.small"))
        }
        if (newSize > MAX_SIZE) {
            throw CommandSyntaxException(
                Component.translatable("commands.worldborder.set.failed.big", Component.text(MAX_SIZE.toString())),
            )
        }
        val old = border.targetSize
        if (timeTicks > 0) {
            val millis = timeTicks.toLong() * MILLIS_PER_TICK
            border.moveSize(newSize, millis)
            broadcast(context, world) { SetBorderLerpSizePacket(old, newSize, millis) }
            context.success(
                Component.translatable(
                    if (newSize > old) "commands.worldborder.set.grow" else "commands.worldborder.set.shrink",
                    Component.text(formatSize(newSize)),
                    Component.text((timeTicks / TICKS_PER_SECOND).toString()),
                ),
                broadcastToAdmins = true,
            )
        } else {
            border.setSize(newSize)
            broadcast(context, world) { SetBorderSizePacket(newSize) }
            context.success(
                Component.translatable(
                    "commands.worldborder.set.immediate",
                    Component.text(formatSize(newSize)),
                ),
                broadcastToAdmins = true,
            )
        }
        return abs(newSize - old).toInt()
    }

    private fun center(context: CommandContext): Int {
        val (world, border) = borderOf(context)
        val coordinates = context.get<Coordinates>("pos")
        val (x, _, z) = coordinates.resolve(context.source.position)
        if (abs(x) > WorldBorder.MAX_CENTER_COORDINATE || abs(z) > WorldBorder.MAX_CENTER_COORDINATE) {
            throw CommandSyntaxException(
                Component.translatable(
                    "commands.worldborder.set.failed.far",
                    Component.text(WorldBorder.MAX_CENTER_COORDINATE.toString()),
                ),
            )
        }
        if (border.centerX == x && border.centerZ == z) {
            throw CommandSyntaxException(Component.translatable("commands.worldborder.center.failed"))
        }
        border.center(x, z)
        broadcast(context, world) { SetBorderCenterPacket(x, z) }
        context.success(
            Component.translatable(
                "commands.worldborder.center.success",
                Component.text(formatSize(x)),
                Component.text(formatSize(z)),
            ),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun damageAmount(context: CommandContext): Int {
        val (_, border) = borderOf(context)
        val amount = context.get<Float>("damagePerBlock").toDouble()
        if (border.damagePerBlock == amount) {
            throw CommandSyntaxException(Component.translatable("commands.worldborder.damage.amount.failed"))
        }
        border.damagePerBlock = amount
        context.success(
            Component.translatable("commands.worldborder.damage.amount.success", Component.text(amount.toString())),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun damageBuffer(context: CommandContext): Int {
        val (_, border) = borderOf(context)
        val distance = context.get<Float>("distance").toDouble()
        if (border.damageBuffer == distance) {
            throw CommandSyntaxException(Component.translatable("commands.worldborder.damage.buffer.failed"))
        }
        border.damageBuffer = distance
        context.success(
            Component.translatable("commands.worldborder.damage.buffer.success", Component.text(distance.toString())),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun warningDistance(context: CommandContext): Int {
        val (world, border) = borderOf(context)
        val distance = context.get<Int>("distance")
        if (border.warningBlocks == distance) {
            throw CommandSyntaxException(Component.translatable("commands.worldborder.warning.distance.failed"))
        }
        border.warningBlocks = distance
        broadcast(context, world) { SetBorderWarningDistancePacket(distance) }
        context.success(
            Component.translatable(
                "commands.worldborder.warning.distance.success",
                Component.text(distance.toString()),
            ),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun warningTime(context: CommandContext): Int {
        val (world, border) = borderOf(context)
        val seconds = context.get<Int>("time")
        if (border.warningSeconds == seconds) {
            throw CommandSyntaxException(Component.translatable("commands.worldborder.warning.time.failed"))
        }
        border.warningSeconds = seconds
        broadcast(context, world) { SetBorderWarningDelayPacket(seconds) }
        context.success(
            Component.translatable("commands.worldborder.warning.time.success", Component.text(seconds.toString())),
            broadcastToAdmins = true,
        )
        return 0
    }

    /** parity: vanilla prints the size with two decimals through `String.format`. */
    private fun formatSize(value: Double): String {
        val scaled = kotlin.math.round(value * 100).toLong()
        val sign = if (scaled < 0) "-" else ""
        val magnitude = kotlin.math.abs(scaled)
        return "$sign${magnitude / 100}." + (magnitude % 100).toString().padStart(2, '0')
    }

    private inline fun <reified T> broadcast(
        context: CommandContext,
        world: LoadedWorld,
        packet: () -> T,
    ) {
        val players = context.environment.playersIn(world)
        if (players.isEmpty()) return
        when (val built = packet()) {
            is SetBorderCenterPacket ->
                for (player in players) {
                    player.connection.sendPlayPacket(SetBorderCenterPacket.ID) { built.write(it) }
                }
            is SetBorderSizePacket ->
                for (player in players) {
                    player.connection.sendPlayPacket(SetBorderSizePacket.ID) { built.write(it) }
                }
            is SetBorderLerpSizePacket ->
                for (player in players) {
                    player.connection.sendPlayPacket(SetBorderLerpSizePacket.ID) { built.write(it) }
                }
            is SetBorderWarningDelayPacket ->
                for (player in players) {
                    player.connection.sendPlayPacket(SetBorderWarningDelayPacket.ID) { built.write(it) }
                }
            is SetBorderWarningDistancePacket ->
                for (player in players) {
                    player.connection.sendPlayPacket(SetBorderWarningDistancePacket.ID) { built.write(it) }
                }
            else -> error("unsupported border packet")
        }
    }

    private companion object {
        const val MIN_SIZE = 1.0
        const val MAX_SIZE = WorldBorder.MAX_SIZE
        const val MILLIS_PER_TICK = 50L
        const val TICKS_PER_SECOND = 20
    }
}
