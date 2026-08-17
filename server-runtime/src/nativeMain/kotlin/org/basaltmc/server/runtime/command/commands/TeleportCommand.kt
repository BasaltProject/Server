package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandTarget
import org.basaltmc.server.runtime.command.Coordinates
import org.basaltmc.server.runtime.command.EntitiesArgument
import org.basaltmc.server.runtime.command.EntityAnchor
import org.basaltmc.server.runtime.command.EntityAnchorArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.RotationArgument
import org.basaltmc.server.runtime.command.Vec3Argument
import org.basaltmc.server.runtime.command.WorldArgument
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.wire.text.Component
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.roundToLong
import kotlin.math.sqrt

/**
 * // parity: TeleportCommand, minus its `facing`, rotation and cross-dimension branches.
 *
 * The three forms keep vanilla's order, which is load-bearing: `<destination>` and `<targets>`
 * both start by parsing a selector, so `/tp Steve 10 64 10` is only reached by failing the first
 * and backtracking.
 *
 * The trailing `<world>` is Basalt's, since vanilla changes world through `/execute`. It is
 * appended rather than inserted so every vanilla form still means what it means in vanilla.
 */
public class TeleportCommand :
    Command(
        name = "teleport",
        permissionLevel = PermissionLevel.GAMEMASTERS,
        aliases = listOf("tp"),
    ) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("location", Vec3Argument) {
            executes { context -> toPosition(context, listOf(context.requireEntity()), context.get("location")) }
            argument("world", WorldArgument) {
                executes { context ->
                    toPosition(context, listOf(context.requireEntity()), context.get("location"), context.get("world"))
                }
            }
        }
        root.argument("destination", PlayersArgument(single = true)) {
            executes { context ->
                toPlayer(
                    context,
                    listOf(context.requireEntity()),
                    context.get<List<PlayerSession>>("destination").single(),
                )
            }
        }
        root.argument("targets", PlayersArgument(single = false)) {
            argument("location", Vec3Argument) {
                executes { context -> toPosition(context, context.get("targets"), context.get("location")) }
                argument("world", WorldArgument) {
                    executes { context ->
                        toPosition(context, context.get("targets"), context.get("location"), context.get("world"))
                    }
                }
                argument("rotation", RotationArgument) {
                    executes { context ->
                        val (yaw, pitch) = context.get<Pair<Float, Float>>("rotation")
                        toPosition(context, context.get("targets"), context.get("location"), facing = yaw to pitch)
                    }
                }
                literal("facing") {
                    argument("facingLocation", Vec3Argument) {
                        executes { context -> toPositionFacing(context, context.get("facingLocation")) }
                    }
                    literal("entity") {
                        argument("facingEntity", EntitiesArgument(single = true)) {
                            executes { context -> toPositionFacingEntity(context, eyes = false) }
                            argument("facingAnchor", EntityAnchorArgument) {
                                executes { context ->
                                    toPositionFacingEntity(
                                        context,
                                        eyes = context.get<EntityAnchor>("facingAnchor") == EntityAnchor.EYES,
                                    )
                                }
                            }
                        }
                    }
                }
            }
            argument("destination", PlayersArgument(single = true)) {
                executes { context ->
                    toPlayer(context, context.get("targets"), context.get<List<PlayerSession>>("destination").single())
                }
            }
        }
    }

    /** parity: `teleport ... facing <pos>`, which turns each target towards a point. */
    private fun toPositionFacing(
        context: CommandContext,
        target: Coordinates,
    ): Int {
        val (x, y, z) = target.resolve(context.source.position)
        return toPosition(context, context.get("targets"), context.get("location"), lookAt = Triple(x, y, z))
    }

    /** parity: `teleport ... facing entity <entity> [<anchor>]`, aiming at its feet or its eyes. */
    private fun toPositionFacingEntity(
        context: CommandContext,
        eyes: Boolean,
    ): Int {
        val entity = context.get<List<CommandTarget>>("facingEntity").single()
        val at = Triple(entity.x, entity.y + if (eyes) EYE_HEIGHT else 0.0, entity.z)
        return toPosition(context, context.get("targets"), context.get("location"), lookAt = at)
    }

    private fun toPosition(
        context: CommandContext,
        targets: List<PlayerSession>,
        coordinates: Coordinates,
        world: LoadedWorld? = null,
        facing: Pair<Float, Float>? = null,
        lookAt: Triple<Double, Double, Double>? = null,
    ): Int {
        // Resolved per target, because `~` means "relative to the entity being moved", not to
        // whoever typed the command. The reported position uses the source's own frame, which is
        // what vanilla prints.
        for (target in targets) {
            val (x, y, z) = coordinates.resolve(Triple(target.x, target.y, target.z))
            // The world change first: it re-sends the position itself, so teleporting afterwards
            // would be a second, redundant move.
            val (yaw, pitch) =
                when {
                    facing != null -> facing
                    lookAt != null -> anglesFrom(x, y, z, lookAt)
                    else -> target.yaw to target.pitch
                }
            if (world == null || !target.connection.switchWorld(world, x, y, z)) {
                target.connection.teleportTo(x, y, z, yaw, pitch)
            }
        }
        val (x, y, z) = coordinates.resolve(context.source.position)
        val message =
            if (targets.size == 1) {
                Component.translatable(
                    "commands.teleport.success.location.single",
                    Component.text(targets.single().name),
                    Component.text(formatCoordinate(x)),
                    Component.text(formatCoordinate(y)),
                    Component.text(formatCoordinate(z)),
                )
            } else {
                Component.translatable(
                    "commands.teleport.success.location.multiple",
                    Component.text(targets.size.toString()),
                    Component.text(formatCoordinate(x)),
                    Component.text(formatCoordinate(y)),
                    Component.text(formatCoordinate(z)),
                )
            }
        context.success(
            if (world ==
                null
            ) {
                message
            } else {
                Component.text("").append(message).append(Component.text(" in ${world.name}"))
            },
            broadcastToAdmins = true,
        )
        return targets.size
    }

    /** parity: `Entity.lookAt`: yaw clockwise from +Z, pitch positive downwards. */
    private fun anglesFrom(
        x: Double,
        y: Double,
        z: Double,
        target: Triple<Double, Double, Double>,
    ): Pair<Float, Float> {
        val dx = target.first - x
        val dy = target.second - (y + EYE_HEIGHT)
        val dz = target.third - z
        val yaw = (-atan2(dx, dz) * DEGREES_PER_RADIAN).toFloat()
        val pitch = (-atan2(dy, sqrt(dx * dx + dz * dz)) * DEGREES_PER_RADIAN).toFloat()
        return yaw to pitch
    }

    private fun toPlayer(
        context: CommandContext,
        targets: List<PlayerSession>,
        destination: PlayerSession,
    ): Int {
        for (target in targets) {
            // parity: `TeleportCommand` moves to the entity, and an entity in another dimension
            // takes you to that dimension. Teleporting the coordinates alone dropped you at their
            // numbers in your own world, next to nobody.
            if (target.worldName != destination.worldName) {
                val world = context.environment.worlds[destination.worldName]
                if (world != null) {
                    target.connection.switchWorld(world, destination.x, destination.y, destination.z)
                }
            }
            target.connection.teleportTo(
                destination.x,
                destination.y,
                destination.z,
                destination.yaw,
                destination.pitch,
            )
        }
        val message =
            if (targets.size == 1) {
                Component.translatable(
                    "commands.teleport.success.entity.single",
                    Component.text(targets.single().name),
                    Component.text(destination.name),
                )
            } else {
                Component.translatable(
                    "commands.teleport.success.entity.multiple",
                    Component.text(targets.size.toString()),
                    Component.text(destination.name),
                )
            }
        context.success(message, broadcastToAdmins = true)
        return targets.size
    }

    /** parity: `TeleportCommand.formatDouble`, which is `String.format("%f")`: always six decimals. */
    private fun formatCoordinate(value: Double): String {
        val scaled = (abs(value) * 1_000_000.0).roundToLong()
        val sign = if (value < 0) "-" else ""
        return "$sign${scaled / 1_000_000}." + (scaled % 1_000_000).toString().padStart(6, '0')
    }

    private companion object {
        const val DEGREES_PER_RADIAN = 57.2957795130823
        const val EYE_HEIGHT = 1.62
    }
}
