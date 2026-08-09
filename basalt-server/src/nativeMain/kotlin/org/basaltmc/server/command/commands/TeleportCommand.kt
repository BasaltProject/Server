package org.basaltmc.server.command.commands

import org.basaltmc.server.command.Command
import org.basaltmc.server.command.CommandContext
import org.basaltmc.server.command.CommandNodeBuilder
import org.basaltmc.server.command.Coordinates
import org.basaltmc.server.command.PermissionLevel
import org.basaltmc.server.command.PlayersArgument
import org.basaltmc.server.command.Vec3Argument
import org.basaltmc.server.command.WorldArgument
import org.basaltmc.server.player.PlayerSession
import org.basaltmc.server.world.LoadedWorld
import org.basaltmc.text.Component
import kotlin.math.abs
import kotlin.math.roundToLong

/**
 * parity: `TeleportCommand`, minus its `facing`, rotation and cross-dimension branches (declared
 * gaps: the first two need a rotation argument, the third needs more than one dimension).
 *
 * The three top-level forms are declared in vanilla's own order, which is load-bearing:
 * `<destination>` and `<targets>` both begin by parsing a player selector, so the dispatcher only
 * reaches `/tp Steve 10 64 10` by failing the first and backtracking into the second.
 *
 * The optional trailing `<world>` is Basalt's, not vanilla's: vanilla changes world with
 * `/execute in <dimension> run tp ...`, which needs `/execute`. It is appended to the existing
 * forms rather than inserted among them precisely so every vanilla form keeps meaning exactly what
 * it means in vanilla: `/tp 10 64 10 lobby` and `/tp Steve 10 64 10 lobby`.
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
            }
            argument("destination", PlayersArgument(single = true)) {
                executes { context ->
                    toPlayer(context, context.get("targets"), context.get<List<PlayerSession>>("destination").single())
                }
            }
        }
    }

    private fun toPosition(
        context: CommandContext,
        targets: List<PlayerSession>,
        coordinates: Coordinates,
        world: LoadedWorld? = null,
    ): Int {
        // Resolved per target, because `~` means "relative to the entity being moved", not to
        // whoever typed the command. The reported position uses the source's own frame, which is
        // what vanilla prints.
        for (target in targets) {
            val (x, y, z) = coordinates.resolve(Triple(target.x, target.y, target.z))
            // The world change first: it re-sends the position itself, so teleporting afterwards
            // would be a second, redundant move.
            if (world == null || !target.connection.switchWorld(world, x, y, z)) {
                target.connection.teleportTo(x, y, z, target.yaw, target.pitch)
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

    private fun toPlayer(
        context: CommandContext,
        targets: List<PlayerSession>,
        destination: PlayerSession,
    ): Int {
        for (target in targets) {
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
}
